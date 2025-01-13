#include <stdlib.h>
#include <string.h>

#include "tool/net/limg.h"
#include "tool/img/lib/image.h"
#include "tool/img/lib/gradienthash.h"

/**
 * Move an ILImageu8 to a Lua-managed userdata object. The original image is invalidated, but its image buffer remains allocated.
 * 
 * This function expects a pointer to the image, so that it can overwrite the original memory with zeroes and prevent double frees or other misuses.
 * 
 * @param L Lua interpreter state.
 * @param image The image to transfer to Lua.
 * @return Number of Lua objects pushed to the stack.
 * @post There's a new ILImageu8 on the Lua stack.
 */
static int LuaImgMakeImageu8(lua_State *L, ILImageu8_t *image) {
  ILImageu8_t *image_lua;
  lua_pushnil(L);
  image_lua = lua_newuserdatauv(L, sizeof(*image_lua), 1);
  luaL_setmetatable(L, "img.Imageu8");
  image_lua->width = image->width;
  image_lua->height = image->height;
  image_lua->channels = image->channels;
  image_lua->data = image->data;
  memset(image, 0, sizeof(*image));
  return 1;
}

static int LuaImgImageu8Free(lua_State *L) {
  ILImageu8_t *image = luaL_checkudata(L, 1, "img.Imageu8");
  ILImageu8Free(image);
  return 0;
}

static int LuaImgLoadFile(lua_State *L) {
  int rc;
  size_t filename_len;
  ILImageu8_t image;
  char const *filename = luaL_checklstring(L, 1, &filename_len);
  rc = ILImageu8LoadFromFile(&image, filename);
  if (!rc) {
    lua_pushnil(L);
    lua_pushstring(L, "out of memory");
    return 2;
  }
  return LuaImgMakeImageu8(L, &image);
}

static int LuaImgLoadBuffer(lua_State *L) {
  int rc;
  size_t buffer_len;
  ILImageu8_t image;
  char const *buffer = luaL_checklstring(L, 1, &buffer_len);
  rc = ILImageu8LoadFromMemory(&image, (unsigned char *)buffer, buffer_len);
  if (!rc) {
    lua_pushnil(L);
    lua_pushstring(L, "out of memory");
    return 2;
  }
  return LuaImgMakeImageu8(L, &image);
}

static int LuaImgImageu8ToString(lua_State *L) {
  char s[128];
  ILImageu8_t *image;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  snprintf(s, sizeof(s), "img.Imageu8(w: %u, h: %u, c: %u, data: %p)", image->width, image->height, image->channels, image->data);
  lua_pushstring(L, s);
  return 1;
}

static int LuaImgImageu8Resize(lua_State *L) {
  int rc;
  ILImageu8_t *image;
  ILImageu8_t resized;
  size_t width, height;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  width = luaL_checkinteger(L, 2);
  if (width < 0) {
    lua_pushfstring(L, "%s must be positive", "width");
    lua_error(L);
    // lua_error doesn't return.
    return 0;
  }
  if (lua_isnoneornil(L, 3)) {
    rc = ILImageu8ResizeToFitSquare(*image, width, &resized);
    if (!rc) {
      lua_pushnil(L);
      lua_pushstring(L, "out of memory");
      return 2;
    }
    // Exit the if intentionally to hit the return at the bottom.
  } else {
    height = luaL_checkinteger(L, 3);
    if (height < 0) {
      lua_pushfstring(L, "%s must be positive", "height");
      lua_error(L);
      // lua_error doesn't return.
      return 0;
    }
    rc = ILImageu8Resize(*image, width, height, &resized);
    if (!rc) {
      lua_pushnil(L);
      lua_pushstring(L, "out of memory");
      return 2;
    }
    // Exit the else intentionally to hit the return at the bottom.
  }
  return LuaImgMakeImageu8(L, &resized);
}

static int LuaImgImageu8SaveFileWebP(lua_State *L) {
  int rc;
  ILImageu8_t *image;
  char const *filename;
  float quality;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  filename = luaL_checkstring(L, 2);
  quality = (float)luaL_optnumber(L, 3, 100.0);
  rc = ILImageu8SaveWebPFile(*image, filename, quality);
  if (!rc) {
    lua_pushnil(L);
    lua_pushstring(L, "failed to save file");
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int LuaImgImageu8SaveBufferWebP(lua_State *L) {
  ILImageu8_t *image;
  int buffer_len;
  unsigned char *buffer;
  float quality;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  quality = (float)luaL_optnumber(L, 2, 100.0);
  buffer = ILImageu8SaveWebPBuffer(*image, quality, &buffer_len);
  if (buffer == NULL) {
    lua_pushnil(L);
    lua_pushstring(L, "failed to encode image");
    return 2;
  }
  lua_pushlstring(L, (char *)buffer, buffer_len);
  return 1;
}

static int LuaImgImageu8SaveFilePNG(lua_State *L) {
  int rc;
  ILImageu8_t *image;
  char const *filename;
  int stride;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  filename = luaL_checkstring(L, 2);
  stride = luaL_optinteger(L, 3, 0);
  rc = ILImageu8SavePNGFile(*image, filename, stride);
  if (!rc) {
    lua_pushnil(L);
    lua_pushstring(L, "failed to save file");
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int LuaImgImageu8SaveBufferPNG(lua_State *L) {
  ILImageu8_t *image;
  int buffer_len;
  unsigned char *buffer;
  int stride;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  stride = luaL_optinteger(L, 3, 0);
  buffer = ILImageu8SavePNGBuffer(*image, stride, &buffer_len);
  if (buffer == NULL) {
    lua_pushnil(L);
    lua_pushstring(L, "failed to encode image");
    return 2;
  }
  lua_pushlstring(L, (char *)buffer, buffer_len);
  return 1;
}

static int LuaImgImageu8SaveFileJPEG(lua_State *L) {
  int rc;
  ILImageu8_t *image;
  char const *filename;
  float quality;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  filename = luaL_checkstring(L, 2);
  quality = (float)luaL_optnumber(L, 3, 100.0);
  rc = ILImageu8SaveJPEGFile(*image, filename, quality);
  if (!rc) {
    lua_pushnil(L);
    lua_pushstring(L, "failed to save file");
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int LuaImgImageu8SaveBufferJPEG(lua_State *L) {
  ILImageu8_t *image;
  int buffer_len;
  unsigned char *buffer;
  int quality;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  quality = (int)luaL_optinteger(L, 2, 100);
  buffer = ILImageu8SaveJPEGBuffer(*image, quality, &buffer_len);
  if (buffer == NULL) {
    lua_pushnil(L);
    lua_pushstring(L, "failed to encode image");
    return 2;
  }
  lua_pushlstring(L, (char *)buffer, buffer_len);
  return 1;
}

static int LuaImgImageu8GradientHash(lua_State *L) {
  int rc;
  uint64_t hash;
  lua_Integer l_hash;
  ILImageu8_t *image;
  image = luaL_checkudata(L, 1, "img.Imageu8");
  rc = GradientHash(*image, &hash);
  if (!rc) {
    lua_pushnil(L);
    lua_pushstring(L, "out of memory");
    return 2;
  }
  l_hash = (int64_t)hash;
  lua_pushinteger(L, l_hash);
  return 1;
}

static int LuaImgImageu8Width(lua_State *L) {
  ILImageu8_t *image = luaL_checkudata(L, 1, "img.Imageu8");
  lua_Integer width = (lua_Integer)image->width;
  lua_pushinteger(L, width);
  return 1;
}

static int LuaImgImageu8Height(lua_State *L) {
  ILImageu8_t *image = luaL_checkudata(L, 1, "img.Imageu8");
  lua_Integer height = (lua_Integer)image->height;
  lua_pushinteger(L, height);
  return 1;
}

static int LuaImgImageu8Channels(lua_State *L) {
  ILImageu8_t *image = luaL_checkudata(L, 1, "img.Imageu8");
  lua_Integer channels = (lua_Integer)image->channels;
  lua_pushinteger(L, channels);
  return 1;
}

static const luaL_Reg kLuaImgImageu8Meta[] = {
  {"__tostring", LuaImgImageu8ToString},
  {"__gc", LuaImgImageu8Free},
  {0},
};

static const luaL_Reg kLuaImgImageu8Meth[] = {
  {"resize", LuaImgImageu8Resize},
  {"savefilewebp", LuaImgImageu8SaveFileWebP},
  {"savebufferwebp", LuaImgImageu8SaveBufferWebP},
  {"savefilepng", LuaImgImageu8SaveFilePNG},
  {"savebufferpng", LuaImgImageu8SaveBufferPNG},
  {"savefilejpeg", LuaImgImageu8SaveFileJPEG},
  {"savebufferjpeg", LuaImgImageu8SaveBufferJPEG},
  {"gradienthash", LuaImgImageu8GradientHash},
  {"width", LuaImgImageu8Width},
  {"height", LuaImgImageu8Height},
  {"channels", LuaImgImageu8Channels},
  {0},
};

static void LuaImgImageu8Obj(lua_State *L) {
  luaL_newmetatable(L, "img.Imageu8");
  luaL_setfuncs(L, kLuaImgImageu8Meta, 0);
  luaL_newlibtable(L, kLuaImgImageu8Meth);
  luaL_setfuncs(L, kLuaImgImageu8Meth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

static const luaL_Reg kLuaImg[] = {
  {"loadfile", LuaImgLoadFile},
  {"loadbuffer", LuaImgLoadBuffer},
  {0},
};

int LuaImg(lua_State *L) {
  luaL_newlib(L, kLuaImg);
  LuaImgImageu8Obj(L);
  return 1;
}