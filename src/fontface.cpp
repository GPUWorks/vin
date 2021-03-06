#include <algorithm>
#include <string>
#include <string.h>
#include <sstream>
#include <cstdlib>
#include <GLFW/glfw3.h>

#include "fontface.h"
#include "util.h"

#ifdef linux

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

#if defined(_WIN32) || defined(WIN32)
std::string get_font_path(const std::string& font_name)
{
    static const char * fontRegistryPath = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    HKEY hKey;
    LONG result;
    std::wstring wsFaceName(font_name.begin(), font_name.end());

    // Open Windows font registry key
    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, fontRegistryPath, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return "";
    }

    DWORD maxValueNameSize, maxValueDataSize;
    result = RegQueryInfoKey(hKey, 0, 0, 0, 0, 0, 0, 0, &maxValueNameSize, &maxValueDataSize, 0, 0);
    if (result != ERROR_SUCCESS) {
        return "";
    }

    DWORD valueIndex = 0;
    LPSTR valueName = new CHAR[maxValueNameSize];
    LPBYTE valueData = new BYTE[maxValueDataSize];
    DWORD valueNameSize, valueDataSize, valueType;
    std::string wsFontFile;

    // Look for a matching font name
    do {
        wsFontFile.clear();
        valueDataSize = maxValueDataSize;
        valueNameSize = maxValueNameSize;

        result = RegEnumValue(hKey, valueIndex, valueName, &valueNameSize, 0, &valueType, valueData, &valueDataSize);

        valueIndex++;

        if (result != ERROR_SUCCESS || valueType != REG_SZ) {
           continue;
        }

        std::string wsValueName(valueName, valueNameSize);
        std::wstring wideValueName;
        for (char c : wsValueName) wideValueName += c;
        std::wstring wideFaceName;
        for (char c : wsFaceName) wideFaceName += c;

        // Found a match
        if (_wcsnicmp(wsFaceName.c_str(), wideValueName.c_str(), wsFaceName.length()) == 0) {
            wsFontFile.assign((LPCSTR)valueData, valueDataSize);
            break;
        }
    } while (result != ERROR_NO_MORE_ITEMS);

    delete[] valueName;
    delete[] valueData;

    RegCloseKey(hKey);

    if (wsFontFile.empty()) {
      return "";
    }

    // Build full font file path
    CHAR winDir[MAX_PATH];
    GetWindowsDirectory(winDir, MAX_PATH);

    std::stringstream ss;
    ss << winDir << "\\Fonts\\" << wsFontFile;
    wsFontFile = ss.str();

    return std::string(wsFontFile.begin(), wsFontFile.end());
}
#endif

#ifdef linux
std::string get_font_path(const std::string& title)
{
    return "";
}
#endif

#ifdef __APPLE__
std::string get_font_path(const std::string& title)
{
    return "";
}
#endif

FontFace::FontFace(FT_Library& library, const std::string& path, unsigned int _height, int _tabs_num_spaces)
    : tabs_num_spaces(_tabs_num_spaces), width(0), cleft(0), height(0)
{
    FT_Face face;
    if (FT_New_Face(library, path.c_str(), 0, &face))
        message_box("Font Error", "Failed to load font: " + path, true);

    // TODO: See whether this is necessary on high-dpi displays (retina, 4k etc)
    /*int mon_width, mon_height;
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    glfwGetMonitorPhysicalSize(monitor, &mon_width, &mon_height);

    double horiz_dpi = mode->width / (mon_width / 25.4);
    double vert_dpi = mode->height / (mon_height / 25.4);

    FT_Set_Char_Size(face, 0, height * 64, (int)horiz_dpi, (int)vert_dpi);*/

    FT_Set_Pixel_Sizes(face, 0, _height); // For now, use this

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte alignment

    // Loop through all ASCII characters and render them to textures
    for (unsigned char c = 1; c < 255; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            error("Failed to load font character '" + std::string{static_cast<char>(c)} + "' from path: " + path);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int hang = face->glyph->bitmap.rows - face->glyph->bitmap_top;
        if (hang > cleft && hang < 500)
            cleft = face->glyph->bitmap.rows - face->glyph->bitmap_top;

        glyphs.emplace(c, Glyph { texture, face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap_left, face->glyph->bitmap_top, face->glyph->advance.x, face->glyph->advance.y });
    }

    // Create a 1-pixel texture which we can upscale and use as a solid rect (e.g for the cursor)
    unsigned char pixel = 0xFF;
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        1,
        1,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        &pixel
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    height = _height + cleft;
    width = face->glyph->advance.x >> 6;

    // TODO: Will using face->glyph->bitmap_top from the last rendered character cause any problems?
    glyphs.emplace(0, Glyph { texture, face->glyph->bitmap.width, static_cast<unsigned int>(height) + cleft,
            0, height, 0, 0 });

    FT_Done_Face(face);
}

int FontFace::font_height() const
{
    return height;
}

int FontFace::font_width() const
{
    return width;
}

int FontFace::font_cleft() const
{
    return cleft;
}

int FontFace::num_spaces() const
{
    return tabs_num_spaces;
}

Glyph FontFace::get_glyph(unsigned char c) const
{
    // TODO: OOB checking / handling
    return glyphs.find(c)->second;
}

std::string FontFace::get_system_font(const std::string& font_name)
{
    return get_font_path(font_name);
}
