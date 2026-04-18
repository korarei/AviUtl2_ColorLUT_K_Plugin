#pragma once

#include <cwctype>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include <wrl/client.h>

#include "direct3d.hpp"
#include "lut.hpp"

struct ID3D11Texture1D;
struct ID3D11Texture3D;
struct ID3D11ShaderResourceView;

namespace lut::filter::cache {
struct LUTData {
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    int dimension = 0;
    uint32_t size = 0;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11Texture1D> lut1d;
    ComPtr<ID3D11Texture3D> lut3d;
};

template <typename T>
class Cache {
public:
    struct Entry {
        std::wstring name;
        T cache;

        constexpr explicit Entry(const std::wstring &n) : name(n) {}
    };

    constexpr ~Cache() = default;
    constexpr Cache() = default;

    constexpr Cache(const Cache &) = delete;
    constexpr Cache &operator=(const Cache &) = delete;
    constexpr Cache(Cache &&) = delete;
    constexpr Cache &operator=(Cache &&) = delete;

    [[nodiscard]] constexpr std::shared_ptr<Entry> fetch(int64_t id, const std::wstring &name) {
        if (auto it = id_to_cache.find(id); it != id_to_cache.end()) {
            if (auto cache = it->second; cache != nullptr && cache->name == name)
                return cache;

            release(id);
        }

        std::shared_ptr<Entry> cache;
        if (auto it = name_to_cache.find(name); it != name_to_cache.end()) {
            cache = it->second.lock();
            if (cache == nullptr)
                name_to_cache.erase(it);
        }

        if (cache == nullptr) {
            cache = std::make_shared<Entry>(name);
            name_to_cache[name] = cache;
        }

        return id_to_cache[id] = std::move(cache);
    }

    constexpr void release(int64_t id) {
        if (auto node = id_to_cache.extract(id)) {
            const auto &cache = node.mapped();
            if (cache != nullptr) {
                if (auto it = name_to_cache.find(cache->name); it != name_to_cache.end() && it->second.expired())
                    name_to_cache.erase(it);
            }
        }
    }

    constexpr void release(const std::wstring &name) {
        if (auto node = name_to_cache.extract(name))
            std::erase_if(id_to_cache, [&](const auto &cache) { return cache.second && cache.second->name == name; });
    }

    constexpr void release() {
        std::unordered_map<std::wstring, std::weak_ptr<Entry>>{}.swap(name_to_cache);
        std::unordered_map<int64_t, std::shared_ptr<Entry>>{}.swap(id_to_cache);
    }

private:
    std::unordered_map<std::wstring, std::weak_ptr<Entry>> name_to_cache;
    std::unordered_map<int64_t, std::shared_ptr<Entry>> id_to_cache;
};

class LUTCache : public Cache<LUTData> {
public:
    template <size_t N, size_t M>
    using Direct3D = direct3d::Direct3D<N, M>;

    struct Info {
        template <typename T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

        int dimension = 0;
        uint32_t size = 0;
        ComPtr<ID3D11ShaderResourceView> srv;
    };

    [[nodiscard]] constexpr bool load(Info &info, int64_t id, const wchar_t *name, const auto &ctrl) {
        const auto lut = fetch(id, name);
        if (lut->cache.dimension != 0 && lut->cache.srv != nullptr) {
            info.dimension = lut->cache.dimension;
            info.size = lut->cache.size;
            info.srv = lut->cache.srv;
            return true;
        }

        const std::filesystem::path path(name);
        auto ext = path.extension().wstring();
        std::ranges::for_each(ext, [](wchar_t &c) { c = std::towlower(c); });

        if (ext == L".cube") {
            CubeLUT cube;
            if (!cube.load(path)) {
                release(id);
                return false;
            }

            lut->cache.dimension = cube.dimension;
            lut->cache.size = cube.size;

            try {
                switch (cube.dimension) {
                    case 1: {
                        ComPtr<ID3D11Texture1D> lut1d;
                        ComPtr<ID3D11ShaderResourceView> srv;
                        ctrl.create_texture(&lut1d, cube.size, 3u, cube.data.data());
                        ctrl.create_srv(&srv, lut1d.Get());
                        lut->cache.lut1d = std::move(lut1d);
                        lut->cache.srv = std::move(srv);
                        break;
                    }
                    case 3: {
                        ComPtr<ID3D11Texture3D> lut3d;
                        ComPtr<ID3D11ShaderResourceView> srv;
                        const auto *data = reinterpret_cast<const pixel::RGBF32 *>(cube.data.data());
                        ctrl.create_texture(&lut3d, cube.size, cube.size, cube.size, data);
                        ctrl.create_srv(&srv, lut3d.Get());
                        lut->cache.lut3d = std::move(lut3d);
                        lut->cache.srv = std::move(srv);
                        break;
                    }
                    default:
                        release(id);
                        throw std::runtime_error("Invalid LUT dimension");
                }
            } catch (...) {
                release(id);
                throw;
            }

            info.dimension = cube.dimension;
            info.size = cube.size;
            info.srv = lut->cache.srv;
            return true;
        } else if (ext == L".bmp" || ext == L".png" || ext == L".tiff" || ext == L".tif") {
            HaldCLUT hald;
            if (!hald.load(path)) {
                release(id);
                return false;
            }

            const auto size = hald.level * hald.level;
            lut->cache.dimension = 3;
            lut->cache.size = size;

            try {
                ComPtr<ID3D11Texture3D> lut3d;
                ComPtr<ID3D11ShaderResourceView> srv;
                ctrl.create_texture(&lut3d, size, size, size, hald.data.data());
                ctrl.create_srv(&srv, lut3d.Get());
                lut->cache.lut3d = std::move(lut3d);
                lut->cache.srv = std::move(srv);
            } catch (...) {
                release(id);
                throw;
            }

            info.dimension = 3;
            info.size = size;
            info.srv = lut->cache.srv;
            return true;
        }

        release(id);
        throw std::runtime_error("Invalid LUT file extension");
    };

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;
};
}  // namespace lut::filter::cache
