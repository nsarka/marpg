#include "map_composition.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <pugixml.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace common {
namespace {
namespace fs = std::filesystem;
using Node = pugi::xml_node;
void require(bool value, const std::string& message) {
    if (!value)
        throw std::runtime_error("Map composition: " + message);
}
Node property(Node node, const char* name) {
    return node.child("properties").find_child_by_attribute("property", "name", name);
}
void number(Node node, const char* name, double value) {
    auto a = node.attribute(name);
    if (!a)
        a = node.append_attribute(name);
    a.set_value(value);
}
struct Tileset {
    unsigned first, count;
    bool leftAligned = false;
    std::map<unsigned, std::pair<double, double>> sizes;
};
struct Composer {
    pugi::xml_document result;
    Node map;
    int width = 0, height = 0;
    double tw = 0, th = 0;
    unsigned nextGid = 1;
    fs::path rootDirectory;
    std::map<std::string, Tileset> sets;
    std::set<std::string> stack;

    std::vector<unsigned> csv(Node layer) {
        auto data = layer.child("data");
        require(std::string(data.attribute("encoding").value()) == "csv" && !data.child("chunk"),
                "prefab tile layers require finite CSV data");
        std::string text = data.text().get();
        std::replace(text.begin(), text.end(), ',', ' ');
        std::istringstream in(text);
        std::vector<unsigned> values;
        unsigned value;
        while (in >> value)
            values.push_back(value);
        require(values.size() == layer.attribute("width").as_uint() * layer.attribute("height").as_uint(),
                "invalid tile data length");
        return values;
    }
    void writeTiles(Node layer, const std::vector<unsigned>& values) {
        auto data = layer.child("data");
        if (!data) {
            data = layer.append_child("data");
            data.append_attribute("encoding") = "csv";
        }
        std::ostringstream out;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i)
                out << ',';
            out << values[i];
        }
        data.text().set(out.str().c_str());
    }
    std::map<unsigned, unsigned> tilesets(Node source, const fs::path& dir) {
        std::map<unsigned, unsigned> gids;
        for (auto ref : source.children("tileset")) {
            require(ref.attribute("source"), "composed maps require external tilesets");
            auto path = fs::weakly_canonical(dir / ref.attribute("source").value()).string();
            auto found = sets.find(path);
            if (found == sets.end()) {
                pugi::xml_document doc;
                require(doc.load_file(path.c_str()), "cannot read tileset " + path);
                auto ts = doc.child("tileset");
                Tileset entry{nextGid, ts.attribute("tilecount").as_uint()};
                require(entry.count > 0, "empty tileset " + path);
                entry.leftAligned =
                    std::string(property(ts, "tile_layer_alignment").attribute("value").value()) ==
                    "bottom_left";
                const std::string alignment = ts.attribute("objectalignment").value();
                require(alignment.empty() || alignment == "unspecified" || alignment == "bottom",
                        "tile objects require bottom-center alignment");
                for (auto tile : ts.children("tile")) {
                    auto image = tile.child("image");
                    entry.sizes[tile.attribute("id").as_uint()] = {
                        image.attribute("width").as_double(ts.attribute("tilewidth").as_double()),
                        image.attribute("height").as_double(ts.attribute("tileheight").as_double())};
                    entry.count = std::max(entry.count, tile.attribute("id").as_uint() + 1);
                }
                nextGid += entry.count;
                auto output = map.append_child("tileset");
                output.append_attribute("firstgid") = entry.first;
                output.append_attribute("source") =
                    fs::relative(path, rootDirectory).generic_string().c_str();
                found = sets.emplace(path, std::move(entry)).first;
            }
            for (unsigned i = 0; i < found->second.count; ++i)
                gids[ref.attribute("firstgid").as_uint() + i] = found->second.first + i;
        }
        return gids;
    }
    unsigned remap(unsigned gid, const std::map<unsigned, unsigned>& gids) {
        if (!gid)
            return 0;
        auto found = gids.find(gid & 0x0fffffff);
        require(found != gids.end(), "unknown tile ID");
        return found->second | (gid & 0xf0000000);
    }
    void roofProperties(Node target, Node source, int cx, int cy) {
        if (source.child("properties"))
            target.append_copy(source.child("properties"));
        auto region = property(target, "roof_region");
        if (region) {
            double x, y, w, h;
            std::istringstream in(region.attribute("value").value());
            require(bool(in >> x >> y >> w >> h) && w > 0 && h > 0, "invalid roof_region");
            std::ostringstream out;
            out << x + cx << ' ' << y + cy << ' ' << w << ' ' << h;
            region.attribute("value") = out.str().c_str();
        }
    }
    Node tileLayer(const std::string& name, Node source, int cx, int cy, double dx, double dy) {
        auto layer = map.append_child("layer");
        layer.append_attribute("name") = name.c_str();
        number(layer, "width", width);
        number(layer, "height", height);
        number(layer, "offsetx", dx);
        number(layer, "offsety", dy);
        roofProperties(layer, source, cx, cy);
        return layer;
    }
    void process(Node source, const fs::path& dir, const std::map<unsigned, unsigned>& gids, int cx, int cy,
                 double dx = 0, double dy = 0) {
        for (auto node : source.children()) {
            const std::string tag = node.name(), name = node.attribute("name").value();
            const double nx = dx + node.attribute("offsetx").as_double(),
                         ny = dy + node.attribute("offsety").as_double();
            if (property(node, "editor_only").attribute("value").as_bool())
                continue;
            if (!node.attribute("visible").as_bool(true))
                continue;
            if (tag == "group") {
                process(node, dir, gids, cx, cy, nx, ny);
                continue;
            }
            if (tag == "layer") {
                auto values = csv(node);
                const int sw = node.attribute("width").as_int();
                const bool shared = name == "Floor" || name == "GroundDetails" || name == "Paving" ||
                                    name == "Walls" || name == "SmallProps";
                Node target;
                if (shared)
                    target = map.find_child_by_attribute("layer", "name", name.c_str());
                auto output = target ? csv(target) : std::vector<unsigned>(width * height);
                if (!target)
                    target = tileLayer(name, node, cx, cy, nx, ny);
                else
                    require(target.attribute("offsetx").as_double() == nx &&
                                target.attribute("offsety").as_double() == ny,
                            "shared layers must have matching offsets");
                for (std::size_t i = 0; i < values.size(); ++i)
                    if (values[i]) {
                        const int x = int(i % sw) + cx, y = int(i / sw) + cy;
                        require(x >= 0 && y >= 0 && x < width && y < height, "prefab extends outside map");
                        output[y * width + x] = remap(values[i], gids);
                    }
                writeTiles(target, output);
            } else if (tag == "objectgroup" && name == "Prefabs") {
                for (auto object : node.children("object")) {
                    auto file = property(object, "prefab");
                    require(file && object.child("point"),
                            "Prefabs requires point objects with a prefab file property");
                    require(object.attribute("rotation").as_double() == 0 && nx == 0 && ny == 0,
                            "prefab markers must be unrotated with no layer offset");
                    const double x = object.attribute("x").as_double() / th,
                                 y = object.attribute("y").as_double() / th;
                    require(x == std::floor(x) && y == std::floor(y),
                            "prefab points must snap to whole map cells");
                    load(dir / file.attribute("value").value(), cx + int(x), cy + int(y));
                }
            } else if (tag == "objectgroup") {
                Node ordinary;
                // Tile objects retain explicit index order. Consecutive objects sharing an
                // offset are batched into a tile layer for the existing rendering pipeline.
                Node batch;
                int previousX = -1, previousY = -1;
                double bx = 0, by = 0;
                std::vector<unsigned> values;
                auto flush = [&] {
                    if (batch)
                        writeTiles(batch, values);
                    batch = {};
                };
                std::vector<Node> objects;
                for (auto object : node.children("object"))
                    objects.push_back(object);
                if (std::string(node.attribute("draworder").value()) != "index")
                    std::stable_sort(objects.begin(), objects.end(), [](Node a, Node b) {
                        return a.attribute("x").as_double() + a.attribute("y").as_double() <
                               b.attribute("x").as_double() + b.attribute("y").as_double();
                    });
                for (auto object : objects) {
                    if (!object.attribute("visible").as_bool(true))
                        continue;
                    if (!object.attribute("gid")) {
                        if (!ordinary) {
                            ordinary = map.append_child("objectgroup");
                            ordinary.append_attribute("name") = name.c_str();
                            number(ordinary, "offsetx", nx);
                            number(ordinary, "offsety", ny);
                            roofProperties(ordinary, node, cx, cy);
                        }
                        auto copy = ordinary.append_copy(object);
                        number(copy, "x", object.attribute("x").as_double() + cx * th);
                        number(copy, "y", object.attribute("y").as_double() + cy * th);
                        continue;
                    }
                    require(object.attribute("rotation").as_double() == 0,
                            "rotated tile objects are not supported");
                    unsigned gid = remap(object.attribute("gid").as_uint(), gids);
                    const Tileset* ts = nullptr;
                    for (auto& [path, set] : sets)
                        if ((gid & 0x0fffffff) >= set.first && (gid & 0x0fffffff) < set.first + set.count)
                            ts = &set;
                    require(ts != nullptr, "missing object tileset");
                    const auto size = ts->sizes.at((gid & 0x0fffffff) - ts->first);
                    require(object.attribute("width").as_double(size.first) == size.first &&
                                object.attribute("height").as_double(size.second) == size.second,
                            "tile objects must use native image dimensions");
                    const double x = object.attribute("x").as_double() + cx * th,
                                 y = object.attribute("y").as_double() + cy * th;
                    const int tx = std::clamp(int(std::floor(x / th)), 0, width - 1),
                              ty = std::clamp(int(std::floor(y / th)), 0, height - 1);
                    // Tiled places painted tiles half a grid width left of its
                    // projected cell origin. Convert tile objects to the same
                    // tile-layer coordinates used by the renderer and collisions.
                    const double ox = (x - y) * tw / (2 * th) + nx - (tx - ty) * tw / 2 -
                                      (ts->leftAligned ? (size.first - tw) / 2 : 0);
                    const double oy = (x + y) / 2 + ny - (tx + ty) * th / 2 - th;
                    if (!batch || std::abs(bx - ox) > 0.001 || std::abs(by - oy) > 0.001 ||
                        values[ty * width + tx] || tx != previousX || ty <= previousY) {
                        flush();
                        batch = tileLayer(name, node, cx, cy, ox, oy);
                        values.assign(width * height, 0);
                        bx = ox;
                        by = oy;
                    }
                    values[ty * width + tx] = gid;
                    previousX = tx;
                    previousY = ty;
                }
                flush();
            }
        }
    }
    void load(const fs::path& input, int cx = 0, int cy = 0) {
        const auto path = fs::weakly_canonical(input);
        require(stack.insert(path.string()).second, "cyclic prefab reference: " + path.string());
        pugi::xml_document doc;
        require(doc.load_file(path.c_str()), "cannot read " + path.string());
        auto source = doc.child("map");
        require(std::string(source.attribute("orientation").value()) == "isometric" &&
                    !source.attribute("infinite").as_bool(),
                "prefabs require finite isometric maps");
        if (!map) {
            rootDirectory = path.parent_path();
            map = result.append_child("map");
            for (auto attr : source.attributes())
                map.append_copy(attr);
            if (source.child("properties"))
                map.append_copy(source.child("properties"));
            width = source.attribute("width").as_int();
            height = source.attribute("height").as_int();
            tw = source.attribute("tilewidth").as_double();
            th = source.attribute("tileheight").as_double();
            require(width > 0 && height > 0 && tw > 0 && th > 0, "invalid map dimensions");
        }
        require(source.attribute("tilewidth").as_double() == tw &&
                    source.attribute("tileheight").as_double() == th,
                "prefab grid differs from parent map");
        auto gids = tilesets(source, path.parent_path());
        // Place prefab contents after the host's terrain so their interiors overlay it.
        pugi::xml_document deferred;
        auto pending = deferred.append_child("pending");
        for (auto node : source.children("objectgroup"))
            if (std::string(node.attribute("name").value()) == "Prefabs")
                pending.append_copy(node);
        for (auto node = source.child("objectgroup"); node;) {
            auto next = node.next_sibling("objectgroup");
            if (std::string(node.attribute("name").value()) == "Prefabs")
                source.remove_child(node);
            node = next;
        }
        process(source, path.parent_path(), gids, cx, cy);
        process(pending, path.parent_path(), gids, cx, cy);
        stack.erase(path.string());
    }
};
} // namespace
std::string composeMap(const std::string& path) {
    pugi::xml_document source;
    require(source.load_file(path.c_str()), "cannot read " + path);
    const auto root = source.child("map");
    bool needed = bool(root.child("group"));
    for (auto layer : root.children("objectgroup")) {
        needed |= std::string(layer.attribute("name").value()) == "Prefabs";
        for (auto object : layer.children("object"))
            needed |= bool(object.attribute("gid"));
    }
    if (!needed)
        return {}; // Preserve tmxlite's full format support for ordinary maps.
    Composer composer;
    composer.load(path);
    // Tilesets must precede all layers, including references imported by prefabs.
    std::vector<Node> references;
    for (auto node : composer.map.children("tileset"))
        references.push_back(node);
    for (auto it = references.rbegin(); it != references.rend(); ++it)
        composer.map.prepend_move(*it);
    // Ground layers imported by prefabs belong before Walls, regardless of import order.
    auto walls = composer.map.find_child_by_attribute("layer", "name", "Walls");
    for (const char* name : {"Floor", "GroundDetails", "Paving"}) {
        auto layer = composer.map.find_child_by_attribute("layer", "name", name);
        if (layer && walls)
            composer.map.insert_move_before(layer, walls);
    }
    unsigned id = 1;
    for (auto node : composer.map.children())
        if (std::string(node.name()) == "layer" || std::string(node.name()) == "objectgroup")
            number(node, "id", id++);
    std::ostringstream out;
    composer.result.save(out);
    return out.str();
}
} // namespace common
