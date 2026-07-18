#pragma once
#include "gl_ext.h"
#include "texture_atlas.h"
#include "../world/block_material.h"
#include "../world/chunk.h"
#include <cmath>

class BuildPreview {
    struct Vertex { float x,y,z,u,v; };
public:
    void init(GLuint atlasTex = 0, float tileSize = 0.0625f, float texel = 0.0039f) {
        atlasTex_=atlasTex; tileSize_=tileSize; texel_=texel;
        static const char* VS=R"(#version 130
in vec3 aPos; in vec2 aUV; uniform mat4 uMVP; uniform vec2 uTileOrigin; uniform float uTileSize; uniform float uTexel; out vec2 vUV;
void main(){gl_Position=uMVP*vec4(aPos,1.0);vUV=uTileOrigin+uTexel*0.5+fract(aUV)*(uTileSize-uTexel);})";
        static const char* FS=R"(#version 130
uniform sampler2D uAtlas; uniform vec4 uTint; in vec2 vUV; out vec4 fragColor;
void main(){vec4 tex=texture2D(uAtlas,vUV); if(tex.a<0.05) discard; fragColor=vec4(tex.rgb*uTint.rgb,tex.a*uTint.a);})";
#ifdef __ANDROID__
        static const char* VS_GLES=R"(
#version 100
precision highp float;
attribute vec3 aPos;
attribute vec2 aUV;
uniform mat4 uMVP;
uniform vec2 uTileOrigin;
uniform float uTileSize;
uniform float uTexel;
varying vec2 vUV;
void main(){gl_Position=uMVP*vec4(aPos,1.0);vUV=uTileOrigin+uTexel*0.5+fract(aUV)*(uTileSize-uTexel);}
)";
        static const char* FS_GLES=R"(
#version 100
precision mediump float;
uniform sampler2D uAtlas;
uniform vec4 uTint;
varying vec2 vUV;
void main(){vec4 tex=texture2D(uAtlas,vUV);if(tex.a<0.05)discard;gl_FragColor=vec4(tex.rgb*uTint.rgb,tex.a*uTint.a);}
)";
#endif
        auto compile=[](GLenum type,const char* src)->GLuint{GLuint s=pglCreateShader(type);pglShaderSource(s,1,&src,nullptr);pglCompileShader(s);GLint ok=0;pglGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[512];pglGetShaderInfoLog(s,sizeof(log),nullptr,log);SDL_Log("[BuildPreview] shader error: %s",log);pglDeleteShader(s);return 0;}return s;};
#ifdef __ANDROID__
        GLuint vs=compile(GL_VERTEX_SHADER,VS_GLES),fs=compile(GL_FRAGMENT_SHADER,FS_GLES);
#else
        GLuint vs=compile(GL_VERTEX_SHADER,VS),fs=compile(GL_FRAGMENT_SHADER,FS);
#endif
        if(!vs||!fs)return;
        program_=pglCreateProgram();pglAttachShader(program_,vs);pglAttachShader(program_,fs);pglLinkProgram(program_);pglDeleteShader(vs);pglDeleteShader(fs);GLint ok=0;pglGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok){char log[512];pglGetProgramInfoLog(program_,sizeof(log),nullptr,log);SDL_Log("[BuildPreview] link error: %s",log);pglDeleteProgram(program_);program_=0;return;}
        locPos_=pglGetAttribLocation(program_,"aPos");locUV_=pglGetAttribLocation(program_,"aUV");locMVP_=pglGetUniformLocation(program_,"uMVP");locAtlas_=pglGetUniformLocation(program_,"uAtlas");locTileOrigin_=pglGetUniformLocation(program_,"uTileOrigin");locTileSize_=pglGetUniformLocation(program_,"uTileSize");locTexel_=pglGetUniformLocation(program_,"uTexel");locTint_=pglGetUniformLocation(program_,"uTint");
        buildUnit_();pglGenVertexArrays(1,&vao_);pglGenBuffers(1,&vbo_);pglBindVertexArray(vao_);pglBindBuffer(GL_ARRAY_BUFFER,vbo_);pglBufferData(GL_ARRAY_BUFFER,sizeof(verts_),verts_,GL_STATIC_DRAW);pglEnableVertexAttribArray((GLuint)locPos_);pglVertexAttribPointer((GLuint)locPos_,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);pglEnableVertexAttribArray((GLuint)locUV_);pglVertexAttribPointer((GLuint)locUV_,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));pglBindVertexArray(0);
    }
    void setAtlas(GLuint tex,float tileSize,float texel){atlasTex_=tex;tileSize_=tileSize;texel_=texel;}
    void draw(float bx,float by,float bz,uint8_t block,bool valid,const float* mvp,float time=0.0f){if(!program_||!atlasTex_||by<0.0f||by>=(float)CHUNK_H)return;const float e=0.006f;Vertex out[36];for(int i=0;i<36;i++){out[i]=verts_[i];out[i].x=bx-e+verts_[i].x*(1.0f+2*e);out[i].y=by-e+verts_[i].y*(1.0f+2*e);out[i].z=bz-e+verts_[i].z*(1.0f+2*e);}float sideU,sideV,topU,topV,bottomU,bottomV;tileOrigin_(Materials::tileForFace(block,2,0),sideU,sideV);tileOrigin_(Materials::tileForFace(block,0,0),topU,topV);tileOrigin_(Materials::tileForFace(block,1,0),bottomU,bottomV);pglUseProgram(program_);pglUniformMatrix4fv(locMVP_,1,GL_FALSE,mvp);pglUniform1i(locAtlas_,0);pglUniform1f(locTileSize_,tileSize_);pglUniform1f(locTexel_,texel_);const float pulse=0.40f+0.035f*sinf(time*5.0f);if(valid)pglUniform4f(locTint_,1.08f,1.08f,1.12f,pulse);else pglUniform4f(locTint_,1.0f,0.52f,0.48f,0.42f);pglActiveTexture(GL_TEXTURE0);pglBindTexture(GL_TEXTURE_2D,atlasTex_);pglBindVertexArray(vao_);pglEnable(GL_BLEND);pglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);pglDepthMask(GL_FALSE);pglUniform2f(locTileOrigin_,sideU,sideV);pglDrawArrays(GL_TRIANGLES,0,24);pglUniform2f(locTileOrigin_,topU,topV);pglDrawArrays(GL_TRIANGLES,24,6);pglUniform2f(locTileOrigin_,bottomU,bottomV);pglDrawArrays(GL_TRIANGLES,30,6);pglDepthMask(GL_TRUE);pglDisable(GL_BLEND);pglBindVertexArray(0);}
    void cleanup(){if(vbo_){pglDeleteBuffers(1,&vbo_);vbo_=0;}if(vao_){pglDeleteVertexArrays(1,&vao_);vao_=0;}if(program_){pglDeleteProgram(program_);program_=0;}}
private:
    GLuint program_=0,vao_=0,vbo_=0,atlasTex_=0;GLint locPos_=-1,locUV_=-1,locMVP_=-1,locAtlas_=-1,locTileOrigin_=-1,locTileSize_=-1,locTexel_=-1,locTint_=-1;float tileSize_=0.0625f,texel_=0.0039f;Vertex verts_[36];
    void tileOrigin_(uint16_t tile,float&u,float&v)const{int col=tile%Atlas::ATLAS_COLS,row=tile/Atlas::ATLAS_COLS;u=(float)(col*Atlas::TILE_STRIDE)/(float)Atlas::ATLAS_PX;v=(float)(row*Atlas::TILE_STRIDE)/(float)Atlas::ATLAS_PX;}
    void addFace_(int&i,const float p[4][3]){const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};const int q[6]={0,1,2,0,2,3};for(int n=0;n<6;n++){int k=q[n];verts_[i++]={p[k][0],p[k][1],p[k][2],uv[k][0],uv[k][1]};}}
    void buildUnit_(){int i=0;const float a[6][4][3]={{{0,0,0},{1,0,0},{1,1,0},{0,1,0}},{{1,0,1},{0,0,1},{0,1,1},{1,1,1}},{{0,0,1},{0,0,0},{0,1,0},{0,1,1}},{{1,0,0},{1,0,1},{1,1,1},{1,1,0}},{{0,1,0},{1,1,0},{1,1,1},{0,1,1}},{{0,0,1},{1,0,1},{1,0,0},{0,0,0}}};for(int f=0;f<6;f++)addFace_(i,a[f]);}
};