#include "line.h"

Line::Line(glm::vec3 start_pos, glm::vec3 end_pos, glm::vec3 col)
{
    color = col;

    shader = Shader("../../shader/line.vert", "../../shader/line.frag");

    glm::vec3 vertices[]{
        start_pos,
        end_pos
    };

    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);

    glNamedBufferData(vbo, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute in the vertex shader
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);

    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(glm::vec3)); // copies vertex data into the buffer's memory
}

Line::~Line()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void Line::render(glm::mat4 mvp)
{
    shader.use();

    shader.setMat4("mvp", mvp);

    shader.setVec3("color", color);

    glBindVertexArray(vao);

    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}
