#include "GLVoxelizerTripiana2009.hpp"

namespace BnZ {

GLVoxelizerTripiana2009::GLVoxelizerTripiana2009(const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "voxskel/Voxskel.vs", "voxskel/Voxskel.gs", "voxskel/Voxskel.fs" })) {
}

GLVoxelizerTripiana2009::~GLVoxelizerTripiana2009() {
}

void GLVoxelizerTripiana2009::initGLState(uint32_t resolution, BBox3f bbox,
                                          GLVoxelFramebuffer& framebuffer, Mat4f& gridToWorldMatrix) {
    auto v = bbox.upper - bbox.lower;

    auto bboxCenter = 0.5f * (bbox.lower + bbox.upper);
    // Grow of 5% the size of the bounding box to ensure that we don't miss any triangle that are at the edge
    bbox.lower = bboxCenter - 0.55f * v;
    bbox.upper = bboxCenter + 0.55f * v;

    auto m_res = resolution;
    // Requiered number of color buffer
    auto m_numRenderTargets = ceil((double)m_res / 128.0);

    auto bboxSize = bbox.size();
    auto m_AABCLength = reduceMax(bboxSize);

    auto m_voxelLength = m_AABCLength / (float)m_res;


    auto m_origBBox = bboxCenter - glm::vec3(0.5f * m_AABCLength);

    gridToWorldMatrix = scale(translate(Mat4f(1.f), m_origBBox), glm::vec3(m_voxelLength));

    // Use the shaders
    m_Program.use();

    // Desactivate depth, cullface
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Blending activated
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_OR);

    // Init FrameBuffer
    if (!framebuffer.init(m_res, m_res, m_res, m_numRenderTargets)){
        std::cerr << "FBO Error" << std::endl;
    }

    //Projection matrix : Adapt the viewport to the size of the mesh
    glm::mat4 P = glm::ortho(-m_AABCLength * 0.5f,
        m_AABCLength * 0.5f,
        -m_AABCLength * 0.5f,
        m_AABCLength * 0.5f,
        0.f,
        m_AABCLength);

    glm::vec3 position(bboxCenter.x, bboxCenter.y, bboxCenter.z + 0.5 * m_AABCLength);
    glm::vec3 point(bboxCenter.x, bboxCenter.y, bboxCenter.z);
    glm::mat4 V = glm::lookAt(position, point, glm::vec3(0, 1, 0));

    // Get the MVP Matrix
    glm::mat4 MVPMatrix = P * V;

    // Set uniforms
    MVP.set(MVPMatrix);
    halfPixelSize.set(Vec2f(1.f / m_res));
    numVoxels.set(resolution);
    origBBox.set(m_origBBox);
    numRenderTargets.set(m_numRenderTargets);
    voxelSize.set(m_voxelLength);

    framebuffer.bind(GL_DRAW_FRAMEBUFFER);

    // Set the list of draw buffers.
    std::vector<GLenum> DrawBuffers(m_numRenderTargets, 0);
    for (int i = 0; i < m_numRenderTargets; ++i){
        DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(m_numRenderTargets, DrawBuffers.data());

    glViewport(0, 0, m_res, m_res);

    // Clear the window
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLVoxelizerTripiana2009::restoreGLState() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glLogicOp(GL_COPY);
    glDisable(GL_COLOR_LOGIC_OP);
}

}
