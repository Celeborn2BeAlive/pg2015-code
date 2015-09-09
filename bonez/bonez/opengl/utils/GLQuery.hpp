#pragma once

#include "GLObject.hpp"

namespace BnZ {

template<GLenum target>
class GLQuery : GLQueryObject<target> {
public:
    using GLQueryObject<target>::glId;

    GLQuery() = default;

    bool isResultAvailable() const {
        if(!m_bHasBegin) {
            return true;
        }
        GLint done;
        glGetQueryObjectiv(glId(), GL_QUERY_RESULT_AVAILABLE, &done);
        return done == GL_TRUE;
    }

    GLuint64 getResult() const {
        if(!m_bHasBegin) {
            return std::numeric_limits<GLuint64>::max();
        }
        GLuint64 result;
        glGetQueryObjectui64v(glId(), GL_QUERY_RESULT, &result);
        return result;
    }

    GLuint64 waitResult() const {
        while(!isResultAvailable()) {
            // do nothing
        }
        return getResult();
    }

    void queryCounter() const {
        glQueryCounter(glId(), GL_TIMESTAMP);
    }

    void begin() {
        m_bHasBegin = true;
        glBeginQuery(target, glId());
    }

private:
    bool m_bHasBegin = false;
};

class GLQueryScope {
    GLenum m_Target;
public:
    template<GLenum target>
    GLQueryScope(GLQuery<target>& query):
        m_Target(target) {
        query.begin();
    }

    ~GLQueryScope() {
        glEndQuery(m_Target);
    }
};

// Query scope to measure time elapsed in nanoseconds
//using GLTimerScope = GLQueryScope<GL_TIME_ELAPSED>;

}
