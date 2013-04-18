#include <ft2build.h>
#include FT_FREETYPE_H
#include <physfs.h>
#include <glm/gtc/type_ptr.hpp>

#include "photon_core.h"
#include "photon_opengl.h"

#define FONT_SIZE 32
#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 128

namespace photon{

namespace gui{
struct character_info {
    glm::vec2 advance;

    float width;
    float rows;
    float left;
    float top;

    float x;
};

struct character_atlas{
    character_info characters[FONT_LAST_CHAR+1];

    int width;

    GLuint texture;
};

// TODO - store this in a better location.
character_atlas main_atlas;

void InitFreeType(){
    PrintToLog("INFO: Initializing FreeType.");
    FT_Library ft;
    FT_Face face;

    if(FT_Init_FreeType(&ft)) {
        PrintToLog("ERROR: Could not init FreeType library!");
        return;
    }

    // TODO - load from some configuration file...
    const char *font_filename = "/Orbitron Bold.otf";

    PHYSFS_File *file;
    intmax_t length;
    uint8_t *font_buffer;

    file = PHYSFS_openRead(font_filename);
    if(!file){
        PrintToLog("ERROR: unable to open Font File: \"%s\"", font_filename);
        return;
    }

    length = PHYSFS_fileLength(file);
    font_buffer = (uint8_t*)malloc(length);

    PHYSFS_read(file, font_buffer, 1, length);
    PHYSFS_close(file);

    FT_Error err = FT_New_Memory_Face(ft, font_buffer, length, 0, &face);

    if(err){
        PrintToLog("ERROR: Could not load font \"%s\"! error code %x!", font_filename, err);
        free(font_buffer);
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, FONT_SIZE);

    FT_GlyphSlot g = face->glyph;
    int width = 0;
    int height = 0;

    for(unsigned char c = FONT_FIRST_CHAR; c <= FONT_LAST_CHAR; c++) {
        if(FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            PrintToLog("WARNING: Loading character %c failed!", c);
            continue;
        }

        width += g->bitmap.width + 1;
        height = std::max(height, g->bitmap.rows);
    }

    main_atlas.width = width;

    glGenTextures(1, &main_atlas.texture);
    glBindTexture(GL_TEXTURE_2D, main_atlas.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    std::vector<GLubyte> empty_image(width * height, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, &empty_image[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    uint32_t x = 0;

    for(unsigned char i = FONT_FIRST_CHAR; i <= FONT_LAST_CHAR; i++) {
        if(FT_Load_Char(face, i, FT_LOAD_RENDER))
            continue;

        glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0, g->bitmap.width, g->bitmap.rows, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);

        main_atlas.characters[i].advance.x = g->advance.x >> 6;
        main_atlas.characters[i].advance.y = g->advance.y >> 6;

        main_atlas.characters[i].width = g->bitmap.width;
        main_atlas.characters[i].rows = g->bitmap.rows;

        main_atlas.characters[i].left = g->bitmap_left;
        main_atlas.characters[i].top = g->bitmap_top;

        main_atlas.characters[i].x = (float)x / (float)width;

        x += g->bitmap.width + 1;
    }

    PrintToLog("INFO: Created character atlas size %ix%i", width,height);

    FT_Done_Face(face);
    free(font_buffer);

    // TODO - perhaps leave this open for loading other fonts and close in a GarbageCollect() function?
    FT_Done_FreeType(ft);
}

void RenderText(glm::vec2 position, glm::vec2 scale, glm::vec4 color, bool center, const char *format,...) {
    va_list args;
    va_start(args, format);

    char buffer[512];
    vsprintf(buffer, format, args);
    va_end(args);

    std::string out(buffer);

    scale /= FONT_SIZE;
    glBindTexture(GL_TEXTURE_2D, main_atlas.texture);

    opengl::SetColorGUI(color);

    if(center){
        glm::vec2 offset(0.0f);
        for(const unsigned char *c = (const unsigned char*)out.c_str(); *c; c++) {
            character_info &character = main_atlas.characters[*c];
            offset += character.advance * scale;
        }
        position -= offset / 2.0f;
    }

    std::vector<glm::vec2> verts;
    std::vector<glm::vec2> uv;

    for(const unsigned char *c = (const unsigned char*)out.c_str(); *c; c++) {
        character_info &character = main_atlas.characters[*c];
        float x =  position.x + character.left * scale.x;
        float y = position.y + character.top * scale.y;
        float w = character.width * scale.x;
        float h = character.rows * scale.y;

        // Advance the cursor to the start of the next character
        position += character.advance * scale;

        // Skip glyphs that have no pixels
        if(!w || !h)
            continue;


        uv.push_back(glm::vec2(character.x, 0.0f));
        verts.push_back(glm::vec2(x, y));

        uv.push_back(glm::vec2(character.x + character.width / main_atlas.width, 0.0f));
        verts.push_back(glm::vec2(x + w, y));

        uv.push_back(glm::vec2(character.x + character.width / main_atlas.width, character.rows / FONT_SIZE));
        verts.push_back(glm::vec2(x + w, y - h));

        uv.push_back(glm::vec2(character.x, character.rows / FONT_SIZE));
        verts.push_back(glm::vec2(x, y - h));
    }

    glVertexAttribPointer(PHOTON_VERTEX_LOCATION_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, 0, glm::value_ptr(verts[0]));
    glVertexAttribPointer(PHOTON_VERTEX_UV_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, 0, glm::value_ptr(uv[0]));

    glDrawArrays(GL_QUADS, 0, verts.size());
}

}

}
