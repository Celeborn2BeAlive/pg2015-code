#pragma once

#include <bonez/opengl/utils/GLutils.hpp>
#include "GLShaderManager.hpp"
#include "GLGBuffer.hpp"
#include <bonez/rendering/Framebuffer.hpp>

namespace BnZ {

class Image;

class GLImageRenderer {
public:
    GLImageRenderer(const GLShaderManager& shaderManager);

    void drawNormalTexture(const GLGBuffer& gBuffer);

    void drawDepthTexture(const GLGBuffer& gBuffer);

    void drawDiffuseTexture(const GLGBuffer& gBuffer);

    void drawGlossyTexture(const GLGBuffer& gBuffer);

    void drawShininessTexture(const GLGBuffer& gBuffer);

    void drawImage(float gamma, const Image& image, bool divideByAlpha = true, bool flipY = false, GLenum filter = GL_LINEAR);

    void drawFramebuffer(float gamma, const Framebuffer& framebuffer, bool flipY = false);

    void drawFramebuffer(float gamma, const Framebuffer& framebuffer, uint32_t channelIdx, bool flipY = false);

    void drawTexture(uint32_t index, const GLTexture2DArray& textureArray);

    void drawDepthTexture(uint32_t index, const GLTexture2DArray& textureArray, float near, float far);

private:
    GLProgram m_Program;

    BNZ_GLUNIFORM(m_Program, GLfloat, uVerticalScale);
    BNZ_GLUNIFORM(m_Program, GLfloat, uGamma);
    BNZ_GLUNIFORM(m_Program, GLSLSampler2Df, uImage);
    BNZ_GLUNIFORM(m_Program, bool, uDisplayAlphaChannel);
    BNZ_GLUNIFORM(m_Program, bool, uDivideByAlpha);

    GLProgram m_DrawTexArrayProgram;

    BNZ_GLUNIFORM(m_DrawTexArrayProgram, GLSLSampler2DArrayf, uTextureArray);
    BNZ_GLUNIFORM(m_DrawTexArrayProgram, GLuint, uLayer);
    BNZ_GLUNIFORM(m_DrawTexArrayProgram, GLfloat, uNear);
    BNZ_GLUNIFORM(m_DrawTexArrayProgram, GLfloat, uFar);
    BNZ_GLUNIFORM(m_DrawTexArrayProgram, bool, uDrawDepth);

    GLScreenTriangle m_ScreenTriangle;
    
};

}
