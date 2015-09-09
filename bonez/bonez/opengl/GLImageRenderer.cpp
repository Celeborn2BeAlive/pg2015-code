#include "GLImageRenderer.hpp"

#include <bonez/image/Image.hpp>

namespace BnZ {

GLImageRenderer::GLImageRenderer(const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "image.vs", "image.fs" })),
    m_DrawTexArrayProgram(shaderManager.buildProgram({ "image.vs", "imageArray.fs", "utils.fs" })) {
}

void GLImageRenderer::drawNormalTexture(const GLGBuffer& gBuffer) {
    glDisable(GL_DEPTH_TEST);

    m_Program.use();

    uGamma.set(1.f);
    gBuffer.getColorBuffer(GBUFFER_NORMAL_DEPTH).bind(0u);
    uImage.set(0u);
    uDisplayAlphaChannel.set(false);
    uDivideByAlpha.set(false);
    uVerticalScale.set(-1.f);

    m_ScreenTriangle.render();
}

void GLImageRenderer::drawDepthTexture(const GLGBuffer& gBuffer) {
    glDisable(GL_DEPTH_TEST);

    m_Program.use();

    uGamma.set(1.f);
    gBuffer.getColorBuffer(GBUFFER_NORMAL_DEPTH).bind(0u);
    uImage.set(0u);
    uDisplayAlphaChannel.set(true);
    uDivideByAlpha.set(false);
    uVerticalScale.set(-1.f);

    m_ScreenTriangle.render();
}

void GLImageRenderer::drawDiffuseTexture(const GLGBuffer& gBuffer) {
    glDisable(GL_DEPTH_TEST);

    m_Program.use();

    uGamma.set(1.f);
    gBuffer.getColorBuffer(GBUFFER_DIFFUSE).bind(0u);
    uImage.set(0u);
    uDisplayAlphaChannel.set(false);
    uDivideByAlpha.set(false);
    uVerticalScale.set(-1.f);

    m_ScreenTriangle.render();
}

void GLImageRenderer::drawGlossyTexture(const GLGBuffer& gBuffer) {
    glDisable(GL_DEPTH_TEST);

    m_Program.use();

    uGamma.set(1.f);
    gBuffer.getColorBuffer(GBUFFER_GLOSSY_SHININESS).bind(0u);
    uImage.set(0u);
    uDisplayAlphaChannel.set(false);
    uDivideByAlpha.set(false);
    uVerticalScale.set(-1.f);

    m_ScreenTriangle.render();
}

void GLImageRenderer::drawShininessTexture(const GLGBuffer& gBuffer) {
    glDisable(GL_DEPTH_TEST);

    m_Program.use();

    uGamma.set(1.f);
    gBuffer.getColorBuffer(GBUFFER_GLOSSY_SHININESS).bind(0u);
    uImage.set(0u);
    uDisplayAlphaChannel.set(true);
    uDivideByAlpha.set(false);
    uVerticalScale.set(-1.f);

    m_ScreenTriangle.render();
}

void GLImageRenderer::drawImage(float gamma, const Image& image, bool divideByAlpha, bool flipY, GLenum filter) {
    glDisable(GL_DEPTH_TEST);

    m_Program.use();

    GLTexture2D m_Texture;
    fillTexture(m_Texture, image);

    m_Texture.setMinFilter(filter);
    m_Texture.setMagFilter(filter);

    uGamma.set(gamma);

    //glActiveTexture(GL_TEXTURE0);
    //m_Texture.bind();

    //auto u = m_Program.getUniformLocation("uImage");
    //glUniform1i(u, 0);
    m_Texture.bind(0u);
    uImage.set(0u);
    uDisplayAlphaChannel.set(false);
    uDivideByAlpha.set(divideByAlpha);
    uVerticalScale.set(flipY ? -1.f : 1.f);

    m_ScreenTriangle.render();
}

void GLImageRenderer::drawFramebuffer(float gamma, const Framebuffer& framebuffer, bool flipY) {
    drawFramebuffer(gamma, framebuffer, 0, flipY);
}

void GLImageRenderer::drawFramebuffer(float gamma, const Framebuffer& framebuffer, uint32_t channelIdx, bool flipY) {
    if(channelIdx < framebuffer.getChannelCount()) {
        drawImage(gamma, framebuffer.getChannel(channelIdx), true, flipY);
    }
}

void GLImageRenderer::drawTexture(uint32_t index, const GLTexture2DArray& textureArray) {
    glDisable(GL_DEPTH_TEST);

    m_DrawTexArrayProgram.use();

    uLayer.set(index);
    textureArray.bind(0u);
    uTextureArray.set(0u);
    uDrawDepth.set(false);

    m_ScreenTriangle.render();
}

void GLImageRenderer::drawDepthTexture(uint32_t index, const GLTexture2DArray& textureArray, float fNear, float fFar) {
    glDisable(GL_DEPTH_TEST);

    m_DrawTexArrayProgram.use();

    uLayer.set(index);
    textureArray.bind(0u);
    uTextureArray.set(0u);
    uDrawDepth.set(true);
    uNear.set(fNear);
    uFar.set(fFar);

    m_ScreenTriangle.render();
}


}
