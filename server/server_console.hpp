#pragma once
#include "common/slash_command.hpp"
#include "connection_manager.hpp"
#include <atomic>
#include <filesystem>
#include <future>
#include <mutex>

// World/navigation construction runs independently. Only the server thread swaps
// the completed world, changes the roster, or touches sockets.
class ServerConsole {
    std::unique_ptr<GameSimulation>& game_;
    ConnectionManager& connections_;
    common::Logger& logger_;
    std::atomic<bool> cancel_{false};
    std::mutex progressMutex_;
    common::LoadStatus latest_;
    std::string loggedStage_;
    double loggedTime_ = -1;
    struct Prepared {
        std::unique_ptr<GameSimulation> world;
        std::unique_ptr<common::Navigation> navigation;
    };
    std::future<Prepared> pending_;
    std::optional<unsigned> pendingBots_;
    void report(const std::string& text) {
        logger_.log_info(text);
        logger_.flush();
    }
    void start(const common::ServerSettings& settings, bool world) {
        if (pending_.valid())
            throw std::runtime_error("A world/navigation load is already in progress");
        loggedStage_.clear();
        loggedTime_ = -1;
        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            latest_ = {"Preparing world"};
        }
        pending_ = std::async(std::launch::async, [this, settings, world] {
            common::LoadProgress progress(
                [this](const auto& value) {
                    std::lock_guard<std::mutex> lock(progressMutex_);
                    latest_ = value;
                },
                [this] { return cancel_.load(); });
            Prepared result;
            if (world)
                result.world = std::make_unique<GameSimulation>(settings, logger_, &progress, false);
            else
                result.navigation = game_->buildNavigation(&progress);
            progress.finish();
            return result;
        });
    }

  public:
    ServerConsole(std::unique_ptr<GameSimulation>& game, ConnectionManager& connections,
                  common::Logger& logger)
        : game_(game), connections_(connections), logger_(logger) {}
    ~ServerConsole() {
        cancel_ = true;
        if (pending_.valid())
            pending_.wait();
    }
    bool busy() const {
        return pending_.valid();
    }
    void execute(const std::string& line) {
        if (line.empty())
            return;
        try {
            const auto command = common::slashCommand(line);
            if (!command) {
                if (common::cleanChatText(line).empty())
                    throw std::runtime_error("Message is empty or too long");
                connections_.sendServerMessage(line);
                return;
            }
            const auto& name = command->name;
            const auto& arg = command->argument;
            if (name == "announce") {
                if (common::cleanChatText(arg).empty())
                    throw std::runtime_error("Usage: /announce <message>");
                connections_.sendServerMessage(arg, common::ChatKind::Announcement);
                return;
            }
            if (busy())
                throw std::runtime_error("Wait for the current world/navigation load to finish");
            if (name == "restart") {
                if (!arg.empty())
                    throw std::runtime_error("Usage: /restart");
                game_->restartRound();
                connections_.replaceSimulation(*game_);
                connections_.sendServerMessage("Round restarted.");
                report("Loading complete. Game started: round restarted.");
            } else if (name == "map") {
                auto settings = game_->settings;
                if (arg.empty() || !std::all_of(arg.begin(), arg.end(), [](unsigned char c) {
                        return std::isalnum(c) || c == '_' || c == '-';
                    }))
                    throw std::runtime_error("Usage: /map <map name without .tmx>");
                settings.map = arg;
                if (!std::filesystem::is_regular_file(common::mapPath(settings)))
                    throw std::runtime_error("Map not found: " + arg);
                pendingBots_.reset();
                start(settings, true);
                report("Preparing " + settings.map + "; current game remains active.");
                connections_.sendServerMessage("Preparing " + settings.map +
                                               ". The round will restart when loading finishes.");
            } else if (name == "bots") {
                auto count = common::commandNumber(arg);
                if (!count || *count > game_->settings.slots)
                    throw std::runtime_error("Usage: /bots <0-" + std::to_string(game_->settings.slots) +
                                             ">");
                const auto humans = std::count_if(game_->players.begin(), game_->players.end(),
                                                  [](const auto& p) { return p.human && p.state.connected; });
                if (*count + humans > game_->settings.slots)
                    throw std::runtime_error("Not enough free slots; human players will not be displaced");
                if (*count && game_->settings.botAI && !game_->hasNavigation()) {
                    pendingBots_ = *count;
                    start(game_->settings, false);
                    report("Preparing bot navigation...");
                } else {
                    game_->setBots(*count);
                    connections_.refreshSettings();
                    report("Bot count: " + std::to_string(*count));
                }
            } else if (name == "ai") {
                if (arg != "on" && arg != "off")
                    throw std::runtime_error("Usage: /ai on|off");
                if (arg == "on" && game_->settings.bots && !game_->hasNavigation()) {
                    pendingBots_.reset();
                    start(game_->settings, false);
                    report("Preparing bot navigation...");
                } else {
                    game_->setAI(arg == "on");
                    connections_.refreshSettings();
                    report("Bot AI: " + arg);
                }
            } else
                throw std::runtime_error(
                    "Commands: /map <name>, /restart, /bots <count>, /ai on|off, /announce <message>");
        } catch (const std::exception& error) {
            logger_.log_error(error.what());
            logger_.flush();
        }
    }
    bool update() {
        if (!pending_.valid())
            return false;
        common::LoadStatus progress;
        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            progress = latest_;
        }
        if (progress.stage != loggedStage_ || progress.totalSeconds - loggedTime_ >= 1) {
            report(progress.stage + " " + std::to_string(progress.completed) + "/" +
                   std::to_string(progress.total) + " (" + std::to_string(progress.totalSeconds) + "s)");
            loggedStage_ = progress.stage;
            loggedTime_ = progress.totalSeconds;
        }
        if (pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return false;
        try {
            auto prepared = pending_.get();
            if (prepared.world) {
                prepared.world->restoreRoster(*game_);
                connections_.replaceSimulation(*prepared.world);
                game_ = std::move(prepared.world);
                connections_.sendServerMessage("Loading complete. Game started on " + game_->settings.map +
                                               ".");
                report("Loading complete. Game started on " + game_->settings.map + ".");
                return true;
            }
            game_->installNavigation(std::move(prepared.navigation));
            if (pendingBots_)
                game_->setBots(*pendingBots_);
            game_->setAI(true);
            connections_.refreshSettings();
            report("Bot navigation ready. AI enabled.");
        } catch (const std::exception& error) {
            logger_.log_error("Command failed: ", error.what());
            logger_.flush();
            connections_.sendServerMessage("World/navigation change failed; current game retained.");
        }
        return false;
    }
};
