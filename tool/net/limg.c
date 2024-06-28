#include "tool/net/limg.h"
#include "tool/img/lib/image.h"
#include "tool/img/lib/gradienthash.h"

static int LuaImgLoadFile(lua_State *L) {
  return 0;
}

static int LuaImgLoadBuffer(lua_State *L) {
  return 0;
}

static int LuaImgImageToString(lua_State *L) {
  return 0;
}

static int LuaImgImageResize(lua_State *L) {
  return 0;
}

static int LuaImgImageSaveFile(lua_State *L) {
  return 0;
}

static int LuaImgImageSaveBuffer(lua_State *L) {
  return 0;
}

static int LuaImgImageGradientHash(lua_State *L) {
  return 0;
}

static const luaL_Reg kLuaImgImageMeta[] = {
  {"__tostring", LuaImgImageToString},
  {0},
};

static const luaL_Reg kLuaImgImageMeth[] = {
  {"resize", LuaImgImageResize},
  {"savefile", LuaImgImageSaveFile},
  {"savebuffer", LuaImgImageSaveBuffer},
  {"gradienthash", LuaImgImageGradientHash},
  {0},
};

static void LuaImgImageObj(lua_State *L) {
  luaL_newmetatable(L, "img.Image");
  luaL_setfuncs(L, kLuaImgImageMeta, 0);
  luaL_newlibtable(L, kLuaImgImageMeth);
  luaL_setfuncs(L, kLuaImgImageMeth, 0);
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
  LuaImgImageObj(L);
  return 1;
}