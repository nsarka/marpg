#pragma once
#include "sound_events.hpp"
#include <SFML/Audio.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>

class SoundSystem {
  public:
    explicit SoundSystem(const std::filesystem::path& directory) {
        std::error_code error;
        std::vector<std::filesystem::path> files;
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end;
             it.increment(error))
            if (it->path().extension() == ".wav")
                files.push_back(it->path());
        if (error)
            std::cerr << "Sound pack unavailable: " << error.message() << '\n';
        std::sort(files.begin(), files.end());
        for (const auto& path : files) {
            const auto category = soundCategory(path.filename().string());
            if (!category)
                continue;
            auto buffer = std::make_unique<sf::SoundBuffer>();
            if (buffer->loadFromFile(path))
                banks_[index(*category)].buffers.push_back(std::move(buffer));
            else
                std::cerr << "Cannot load sound: " << path << '\n';
        }
        for (const auto& bank : banks_)
            if (bank.buffers.empty())
                std::cerr << "Sound effect group has no playable variants\n";
    }
    std::size_t variantCount(SoundEffect effect) const {
        return banks_[index(effect)].buffers.size();
    }
    void observe(const std::vector<common::PlayerState>& players, sf::Vector2f listener) {
        voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                     [](const auto& voice) {
                                         return voice.sound->getStatus() == sf::SoundSource::Status::Stopped;
                                     }),
                      voices_.end());
        for (const auto& cue : events_.observe(players))
            play(cue, listener);
        for (auto& voice : voices_)
            mix(voice, listener);
    }
    void play(SoundCue cue, sf::Vector2f listener) {
        if (common::distance(cue.position, listener) >= 900.f)
            return;
        auto& bank = banks_[index(cue.effect)];
        if (bank.buffers.empty())
            return;
        std::size_t choice = 0;
        if (bank.buffers.size() > 1) {
            const bool excludeLast = bank.last.has_value();
            choice =
                std::uniform_int_distribution<std::size_t>(0, bank.buffers.size() - 1 - excludeLast)(random_);
            if (excludeLast && choice >= *bank.last)
                ++choice;
        }
        bank.last = choice;
        // Bound simultaneous voices while keeping buffers alive for every active sound.
        if (voices_.size() >= 32)
            voices_.erase(voices_.begin());
        Voice voice{std::make_unique<sf::Sound>(*bank.buffers[choice]), cue.position,
                    cue.effect == SoundEffect::Cast    ? 32.f
                    : cue.effect == SoundEffect::Swing ? 38.f
                                                       : 52.f};
        voice.sound->setSpatializationEnabled(false);
        mix(voice, listener);
        voice.sound->play();
        voices_.push_back(std::move(voice));
    }

  private:
    static std::size_t index(SoundEffect effect) {
        return static_cast<std::size_t>(effect);
    }
    struct Bank {
        std::vector<std::unique_ptr<sf::SoundBuffer>> buffers;
        std::optional<std::size_t> last;
    };
    struct Voice {
        std::unique_ptr<sf::Sound> sound;
        sf::Vector2f position;
        float volume;
    };
    static void mix(Voice& voice, sf::Vector2f listener) {
        const float distance = common::distance(voice.position, listener);
        const float fade = std::clamp(1.f - std::max(0.f, distance - 80.f) / 820.f, 0.f, 1.f);
        voice.sound->setVolume(voice.volume * fade * fade);
        voice.sound->setPan(std::clamp((voice.position.x - listener.x) / 600.f, -0.8f, 0.8f));
    }
    std::array<Bank, static_cast<std::size_t>(SoundEffect::Count)> banks_;
    std::vector<Voice> voices_; // destroyed before their buffers
    std::mt19937 random_{std::random_device{}()};
    SoundEvents events_;
};
