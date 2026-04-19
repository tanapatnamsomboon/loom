#include "loom/renderer/subtexture_2d.h"

namespace Loom {

    SubTexture2D::SubTexture2D(const std::shared_ptr<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max)
        : mTexture(texture) {
        mTexCoords[0] = { min.x, min.y };
        mTexCoords[1] = { max.x, min.y };
        mTexCoords[2] = { max.x, max.y };
        mTexCoords[3] = { min.x, max.y };
    }

    std::shared_ptr<SubTexture2D> SubTexture2D::CreateFromCoords(const std::shared_ptr<Texture2D>& texture, const glm::vec2& coords, const glm::vec2& cell_size, const glm::vec2& sprite_size) {
        glm::vec2 min = { coords.x * cell_size.x / texture->GetWidth(), coords.y * cell_size.y / texture->GetHeight() };
        glm::vec2 max = { (coords.x + sprite_size.x) * cell_size.x / texture->GetWidth(),
                        (coords.y + sprite_size.y) * cell_size.y / texture->GetHeight() };

        return std::make_shared<SubTexture2D>(texture, min, max);
    }

} // namespace Loom