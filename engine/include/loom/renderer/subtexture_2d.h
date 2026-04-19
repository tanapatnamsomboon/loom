#pragma once

#include "loom/renderer/texture.h"
#include <glm/glm.hpp>
#include <memory>

namespace Loom {

    class LOOM_API SubTexture2D {
    public:
        SubTexture2D(const std::shared_ptr<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max);

        std::shared_ptr<Texture2D> GetTexture() const { return mTexture; }
        const glm::vec2* GetTexCoords() const { return mTexCoords; }

        static std::shared_ptr<SubTexture2D> CreateFromCoords(
            const std::shared_ptr<Texture2D>& texture,
            const glm::vec2&                  coords,
            const glm::vec2&                  cell_size,
            const glm::vec2&                  sprite_size = { 1, 1 }
        );

    private:
        std::shared_ptr<Texture2D> mTexture;
        glm::vec2 mTexCoords[4];
    };

} // namespace Loom