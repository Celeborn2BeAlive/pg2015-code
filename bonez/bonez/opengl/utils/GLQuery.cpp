#include "GLQuery.hpp"

namespace BnZ {

//GLQuery::GLQuery() {
//    glBeginQuery(GL_TIME_ELAPSED, glId());
//    glEndQuery(GL_TIME_ELAPSED);
//}

//bool GLQuery::isResultAvailable() const {
//    if(!m_bHasBegin) {
//        return true;
//    }
//    GLint done;
//    glGetQueryObjectiv(glId(), GL_QUERY_RESULT_AVAILABLE, &done);
//    return done == GL_TRUE;
//}

//GLuint64 GLQuery::getResult() const {
//    if(!m_bHasBegin) {
//        return std::numeric_limits<GLuint64>::max();
//    }
//    GLuint64 result;
//    glGetQueryObjectui64v(glId(), GL_QUERY_RESULT, &result);
//    return result;
//}

//GLuint64 GLQuery::waitResult() const {
//    while(!isResultAvailable()) {
//        // do nothing
//    }
//    return getResult();
//}

//void GLQuery::queryCounter() const {
//    glQueryCounter(glId(), GL_TIMESTAMP);
//}

//void GLQuery::begin(GLenum type) {
//    m_bHasBegin = true;
//    glBeginQuery(type, glId());
//}

}
