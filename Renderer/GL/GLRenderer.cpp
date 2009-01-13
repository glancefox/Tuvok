/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
  \file    GLRenderer.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#include "GLRenderer.h"
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>
#include <ctime>

using namespace std;

GLRenderer::GLRenderer(MasterController* pMasterController, bool bUseOnlyPowerOfTwo) : 
  AbstrRenderer(pMasterController, bUseOnlyPowerOfTwo),
  m_fScaledIsovalue(0.0f),    // set by StartFrame
  m_fScaledCVIsovalue(0.0f),  // set by StartFrame
  m_p1DTransTex(NULL),
  m_p2DTransTex(NULL),
  m_p1DData(NULL),
  m_p2DData(NULL),
  m_pFBO3DImageLast(NULL),
  m_iFilledBuffers(0),
  m_pLogoTex(NULL),
  m_pProgramIso(NULL),
  m_pProgramHQMIPRot(NULL),
  m_pProgramTrans(NULL),
  m_pProgram1DTransSlice(NULL),
  m_pProgram2DTransSlice(NULL),
  m_pProgramMIPSlice(NULL),
  m_pProgramTransMIP(NULL),
  m_pProgramIsoCompose(NULL),
  m_pProgramCVCompose(NULL),
  m_pProgramComposeAnaglyphs(NULL)
{
  m_pProgram1DTrans[0]   = NULL;
  m_pProgram1DTrans[1]   = NULL;
  m_pProgram2DTrans[0]   = NULL;
  m_pProgram2DTrans[1]   = NULL;

  m_pFBO3DImageCurrent[0] = NULL;
  m_pFBOIsoHit[0] = NULL;
  m_pFBOCVHit[0] = NULL;
  m_pFBO3DImageCurrent[1] = NULL;
  m_pFBOIsoHit[1] = NULL;
  m_pFBOCVHit[1] = NULL;

}

GLRenderer::~GLRenderer() {
  delete [] m_p1DData;
  delete [] m_p2DData;
}

bool GLRenderer::Initialize() {
  if (!AbstrRenderer::Initialize()) {
    m_pMasterController->DebugOut()->Error("GLRenderer::Initialize","Error in parent call -> aborting");
    return false;
  }

  string strPotential1DTransName = SysTools::ChangeExt(m_pDataset->Filename(), "1dt");
  string strPotential2DTransName = SysTools::ChangeExt(m_pDataset->Filename(), "2dt");
  
  if (SysTools::FileExists(strPotential1DTransName)) 
    m_pMasterController->MemMan()->Get1DTransFromFile(strPotential1DTransName, this, &m_p1DTrans, &m_p1DTransTex);
  else
    m_pMasterController->MemMan()->GetEmpty1DTrans(m_pDataset->Get1DHistogram()->GetFilledSize(), this, &m_p1DTrans, &m_p1DTransTex);

  if (SysTools::FileExists(strPotential2DTransName)) 
    m_pMasterController->MemMan()->Get2DTransFromFile(strPotential2DTransName, this, &m_p2DTrans, &m_p2DTransTex);
  else {
    m_pMasterController->MemMan()->GetEmpty2DTrans(m_pDataset->Get2DHistogram()->GetFilledSize(), this, &m_p2DTrans, &m_p2DTransTex);

    TFPolygon newSwatch;
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.1f,0.1f));
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.1f,0.9f));
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.9f,0.9f));
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.9f,0.1f));

    newSwatch.pGradientCoords[0] = FLOATVECTOR2(0.1f,0.5f);
    newSwatch.pGradientCoords[1] = FLOATVECTOR2(0.9f,0.5f);

    GradientStop g1(0,FLOATVECTOR4(0,0,0,0)),g2(0.5f,FLOATVECTOR4(1,1,1,1)),g3(1,FLOATVECTOR4(0,0,0,0));
    newSwatch.pGradientStops.push_back(g1);
    newSwatch.pGradientStops.push_back(g2);
    newSwatch.pGradientStops.push_back(g3);

    m_p2DTrans->m_Swatches.push_back(newSwatch);
    m_pMasterController->MemMan()->Changed2DTrans(NULL, m_p2DTrans);
  }

  if (!LoadAndVerifyShader("Transfer-VS.glsl", "Transfer-FS.glsl",     m_vShaderSearchDirs, &(m_pProgramTrans))        ||
      !LoadAndVerifyShader("Transfer-VS.glsl", "1D-slice-FS.glsl",     m_vShaderSearchDirs, &(m_pProgram1DTransSlice)) ||
      !LoadAndVerifyShader("Transfer-VS.glsl", "2D-slice-FS.glsl",     m_vShaderSearchDirs, &(m_pProgram2DTransSlice)) ||
      !LoadAndVerifyShader("Transfer-VS.glsl", "MIP-slice-FS.glsl",    m_vShaderSearchDirs, &(m_pProgramMIPSlice))     ||
      !LoadAndVerifyShader("Transfer-VS.glsl", "Transfer-MIP-FS.glsl", m_vShaderSearchDirs, &(m_pProgramTransMIP))     ||
      !LoadAndVerifyShader("Transfer-VS.glsl", "Compose-FS.glsl",      m_vShaderSearchDirs, &(m_pProgramIsoCompose))   ||
      !LoadAndVerifyShader("Transfer-VS.glsl", "Compose-CV-FS.glsl",   m_vShaderSearchDirs, &(m_pProgramCVCompose))    ||
      !LoadAndVerifyShader("Transfer-VS.glsl", "Compose-Anaglyphs-FS.glsl",m_vShaderSearchDirs, &(m_pProgramComposeAnaglyphs)))   {

      m_pMasterController->DebugOut()->Error("GLRenderer::Initialize","Error loading transfer shaders.");
      return false;
  } else {
    m_pProgramTrans->Enable();
    m_pProgramTrans->SetUniformVector("texColor",0);
    m_pProgramTrans->SetUniformVector("texDepth",1);
    m_pProgramTrans->Disable();

    m_pProgram1DTransSlice->Enable();
    m_pProgram1DTransSlice->SetUniformVector("texVolume",0);
    m_pProgram1DTransSlice->SetUniformVector("texTrans1D",1);
    m_pProgram1DTransSlice->Disable();

    m_pProgram2DTransSlice->Enable();
    m_pProgram2DTransSlice->SetUniformVector("texVolume",0);
    m_pProgram2DTransSlice->SetUniformVector("texTrans2D",1);
    m_pProgram2DTransSlice->Disable();

    m_pProgramMIPSlice->Enable();
    m_pProgramMIPSlice->SetUniformVector("texVolume",0);
    m_pProgramMIPSlice->Disable();

    m_pProgramTransMIP->Enable();
    m_pProgramTransMIP->SetUniformVector("texLast",0);
    m_pProgramTransMIP->SetUniformVector("texTrans1D",1);
    m_pProgramTransMIP->Disable();

    FLOATVECTOR2 vParams = m_FrustumCullingLOD.GetDepthScaleParams();

    m_pProgramIsoCompose->Enable();
    m_pProgramIsoCompose->SetUniformVector("texRayHitPos",0);
    m_pProgramIsoCompose->SetUniformVector("texRayHitNormal",1);
    m_pProgramIsoCompose->SetUniformVector("vLightAmbient",0.2f,0.2f,0.2f);
    m_pProgramIsoCompose->SetUniformVector("vLightDiffuse",0.8f,0.8f,0.8f);
    m_pProgramIsoCompose->SetUniformVector("vLightSpecular",1.0f,1.0f,1.0f);
    m_pProgramIsoCompose->SetUniformVector("vLightDir",0.0f,0.0f,-1.0f);
    m_pProgramIsoCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);
    m_pProgramIsoCompose->Disable();

    m_pProgramCVCompose->Enable();
    m_pProgramCVCompose->SetUniformVector("texRayHitPos",0);
    m_pProgramCVCompose->SetUniformVector("texRayHitNormal",1);
    m_pProgramCVCompose->SetUniformVector("texRayHitPos2",2);
    m_pProgramCVCompose->SetUniformVector("texRayHitNormal2",3);
    m_pProgramCVCompose->SetUniformVector("vLightAmbient",0.2f,0.2f,0.2f);
    m_pProgramCVCompose->SetUniformVector("vLightDiffuse",0.8f,0.8f,0.8f);
    m_pProgramCVCompose->SetUniformVector("vLightSpecular",1.0f,1.0f,1.0f);
    m_pProgramCVCompose->SetUniformVector("vLightDir",0.0f,0.0f,-1.0f);
    m_pProgramCVCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);
    m_pProgramCVCompose->Disable();

    m_pProgramComposeAnaglyphs->Enable();
    m_pProgramComposeAnaglyphs->SetUniformVector("texLeftEye",0);
    m_pProgramComposeAnaglyphs->SetUniformVector("texRightEye",1);
    m_pProgramComposeAnaglyphs->Disable();
    
  }

  return true;
}

void GLRenderer::Changed1DTrans() {
  m_p1DTrans->GetByteArray(&m_p1DData);
  m_p1DTransTex->SetData(m_p1DData);

  AbstrRenderer::Changed1DTrans();
}

void GLRenderer::Changed2DTrans() {
  m_p2DTrans->GetByteArray(&m_p2DData);
  m_p2DTransTex->SetData(m_p2DData);

  AbstrRenderer::Changed2DTrans();
}

void GLRenderer::Resize(const UINTVECTOR2& vWinSize) {
  AbstrRenderer::Resize(vWinSize);
  m_pMasterController->DebugOut()->Message("GLRenderer::Resize","Resizing to %i x %i", vWinSize.x, vWinSize.y);
  CreateOffscreenBuffers();
}

void GLRenderer::RenderSeperatingLines() {
  m_pFBO3DImageCurrent[0]->Write();
  // set render area to fullscreen
  SetRenderTargetAreaScissor(RA_FULLSCREEN);
  SetRenderTargetArea(RA_FULLSCREEN);

  // render seperating lines
  glDisable(GL_BLEND);

  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-1, 1, 1, -1, 0, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glBegin(GL_LINES);
    glColor4f(1.0f,1.0f,1.0f,1.0f);
    glVertex3f(0,-1,0);
    glVertex3f(0,1,0);
    glVertex3f(-1,0,0);
    glVertex3f(1,0,0);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  m_pFBO3DImageCurrent[0]->FinishWrite();
}

void GLRenderer::ClearDepthBuffer() {
  glClear(GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::ClearColorBuffer() {
  glDepthMask(GL_FALSE);
  if (m_bDoStereoRendering) {
    // render anaglyphs agains a black background only
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT); 
  } else {
    if (m_vBackgroundColors[0] == m_vBackgroundColors[1]) {
      glClearColor(m_vBackgroundColors[0].x,m_vBackgroundColors[0].y,m_vBackgroundColors[0].z,0);
      glClear(GL_COLOR_BUFFER_BIT); 
    } else {
      glDisable(GL_BLEND);
      DrawBackGradient();
    }
  }
  DrawLogo();
  glDepthMask(GL_TRUE);
}


void GLRenderer::StartFrame() {
  // clear the framebuffer (if requested)
  if (m_bClearFramebuffer) ClearDepthBuffer();

  if (m_eRenderMode == RM_ISOSURFACE) {
    FLOATVECTOR2 vfWinSize = FLOATVECTOR2(m_vWinSize);
    if (m_bDoClearView) {
      m_pProgramCVCompose->Enable();
      m_pProgramCVCompose->SetUniformVector("vScreensize",vfWinSize.x, vfWinSize.y);
      m_pProgramCVCompose->Disable();
    } else {
      m_pProgramIsoCompose->Enable();
      m_pProgramIsoCompose->SetUniformVector("vScreensize",vfWinSize.x, vfWinSize.y);
      m_pProgramIsoCompose->Disable();
    } 

    size_t       iMaxValue  = m_p1DTrans->GetSize();
    UINT32 iMaxRange        = UINT32(1<<m_pDataset->GetInfo()->GetBitwith());
    m_fScaledIsovalue       = m_fIsovalue * float(iMaxValue)/float(iMaxRange);
    m_fScaledCVIsovalue     = m_fCVIsovalue * float(iMaxValue)/float(iMaxRange);

  }
}

void GLRenderer::Paint() {
  AbstrRenderer::Paint();

  StartFrame();

  bool bNewDataToShow = false;
  if (m_eViewMode == VM_SINGLE) {
    // set render area to fullscreen
    SetRenderTargetArea(RA_FULLSCREEN);

    switch (m_eFullWindowMode) {
       case WM_3D       : {
                              if (!m_bPerformRedraw && m_bPerformReCompose){
                                Recompose3DView(RA_FULLSCREEN);
                                bNewDataToShow = true;
                              } else {
                                // plan the frame
                                Plan3DFrame();
                                // execute the frame
                                bNewDataToShow = Execute3DFrame(RA_FULLSCREEN); 
                              }
                              break;
                          }
       case WM_SAGITTAL : 
       case WM_AXIAL    : 
       case WM_CORONAL  : if (m_bPerformRedraw) bNewDataToShow = Render2DView(RA_FULLSCREEN, m_eFullWindowMode, m_piSlice[size_t(m_eFullWindowMode)]); break;
       default          : m_pMasterController->DebugOut()->Error("GLRenderer::Paint","Invalid Windowmode");
                          bNewDataToShow = false;
                          break;

    }

  } else { // VM_TWOBYTWO 
    int iActiveRenderWindows = 0;
    int iReadyWindows = 0;

    for (UINT32 i = 0;i<4;i++) {
      ERenderArea eArea = ERenderArea(int(RA_TOPLEFT)+i);

      if (m_bRedrawMask[size_t(m_e2x2WindowMode[i])]) {
        iActiveRenderWindows++;
        SetRenderTargetArea(eArea);
        bool bLocalNewDataToShow;
        switch (m_e2x2WindowMode[i]) {
           case WM_3D       : {
                                if (!m_bPerformRedraw && m_bPerformReCompose){
                                  Recompose3DView(eArea);
                                  bLocalNewDataToShow = true;
                                } else {
                                  // plan the frame
                                  Plan3DFrame();
                                  // execute the frame
                                  bLocalNewDataToShow = Execute3DFrame(eArea);
                                }
                                // are we done traversing the LOD levels
                                m_bRedrawMask[size_t(m_e2x2WindowMode[i])] = (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame) || (m_iCurrentLODOffset > m_iMinLODForCurrentView);
                                break;
                              }
           case WM_SAGITTAL : 
           case WM_AXIAL    : 
           case WM_CORONAL  : bLocalNewDataToShow= Render2DView(eArea, m_e2x2WindowMode[i], m_piSlice[size_t(m_e2x2WindowMode[i])]); 
                              m_bRedrawMask[size_t(m_e2x2WindowMode[i])] = false;
                              break;
           default          : m_pMasterController->DebugOut()->Error("GLRenderer::Paint","Invalid Windowmode");
                              bLocalNewDataToShow = false; 
                              break;
        }
        
        if (bLocalNewDataToShow) iReadyWindows++;
      } else {
        // blit the previous result quad to the entire screen but restrict draing to the current subarea
        m_pFBO3DImageCurrent[0]->Write();
        GLFBOTex::OneDrawBuffer();
        SetRenderTargetArea(RA_FULLSCREEN);
        SetRenderTargetAreaScissor(eArea);
        RerenderPreviousResult(false);
        m_pFBO3DImageCurrent[0]->FinishWrite();
      }
    }

    // if we had at least one renderwindow that was doing something and from those all are finished then 
    // set a flag so that we can display the result to the user later
    bNewDataToShow = (iActiveRenderWindows > 0) && (iReadyWindows==iActiveRenderWindows);

    // render cross to seperate the four subwindows
    RenderSeperatingLines();
  }

  EndFrame(bNewDataToShow);
}

void GLRenderer::EndFrame(bool bNewDataToShow) {
  // if the image is complete
  if (bNewDataToShow) {

    // in stereo compose both images into one, in mono mode simply swap the pointers
    if (m_bDoStereoRendering) {
      m_pFBO3DImageCurrent[0]->Read(0);
      m_pFBO3DImageCurrent[1]->Read(1);

      m_pFBO3DImageLast->Write(0, 0);
      GLFBOTex::OneDrawBuffer();
      glClear(GL_COLOR_BUFFER_BIT);


      m_pProgramComposeAnaglyphs->Enable();
      glDisable(GL_DEPTH_TEST);
      glBegin(GL_QUADS);
        glTexCoord2d(0,0);
        glVertex3d(-1.0, -1.0, -0.5);
        glTexCoord2d(1,0);
        glVertex3d( 1.0, -1.0, -0.5);
        glTexCoord2d(1,1);
        glVertex3d( 1.0,  1.0, -0.5);
        glTexCoord2d(0,1);
        glVertex3d(-1.0,  1.0, -0.5);
      glEnd();
      m_pProgramComposeAnaglyphs->Disable();

      m_pFBO3DImageLast->FinishWrite();

      m_pFBO3DImageCurrent[0]->FinishRead();
      m_pFBO3DImageCurrent[1]->FinishRead();
    } else {
      swap(m_pFBO3DImageLast, m_pFBO3DImageCurrent[0]);
    }
    m_iFilledBuffers = 0;
  }

  // show the result
  if (bNewDataToShow || m_iFilledBuffers < 2) 
    RerenderPreviousResult(true);

  // no complete redraw is necessary as we just finished the first pass
  m_bPerformRedraw = false;
}


void GLRenderer::SetRenderTargetArea(ERenderArea eREnderArea) {
  switch (eREnderArea) {
    case RA_TOPLEFT     : SetViewPort(UINTVECTOR2(0,m_vWinSize.y/2), UINTVECTOR2(m_vWinSize.x/2,m_vWinSize.y)); break;
    case RA_TOPRIGHT    : SetViewPort(m_vWinSize/2, m_vWinSize); break;
    case RA_LOWERLEFT   : SetViewPort(UINTVECTOR2(0,0),m_vWinSize/2); break;
    case RA_LOWERRIGHT  : SetViewPort(UINTVECTOR2(m_vWinSize.x/2,0), UINTVECTOR2(m_vWinSize.x,m_vWinSize.y/2)); break;
    case RA_FULLSCREEN  : SetViewPort(UINTVECTOR2(0,0), m_vWinSize); break;
    default             : m_pMasterController->DebugOut()->Error("GLRenderer::SetRenderTargetArea","Invalid render area set"); break;
  }
}

void GLRenderer::SetRenderTargetAreaScissor(ERenderArea eREnderArea) {
  switch (eREnderArea) {
    case RA_TOPLEFT     : glScissor(0,m_vWinSize.y/2, m_vWinSize.x/2,m_vWinSize.y); glEnable( GL_SCISSOR_TEST ); break;
    case RA_TOPRIGHT    : glScissor(m_vWinSize.x/2, m_vWinSize.y/2, m_vWinSize.x, m_vWinSize.y); glEnable( GL_SCISSOR_TEST );break;
    case RA_LOWERLEFT   : glScissor(0,0,m_vWinSize.x/2, m_vWinSize.y/2); glEnable( GL_SCISSOR_TEST );break;
    case RA_LOWERRIGHT  : glScissor(m_vWinSize.x/2,0,m_vWinSize.x,m_vWinSize.y/2); glEnable( GL_SCISSOR_TEST );break;
    case RA_FULLSCREEN  : /*glScissor(0,0,m_vWinSize.x, m_vWinSize.y);*/ glDisable( GL_SCISSOR_TEST );break;
    default             : m_pMasterController->DebugOut()->Error("GLRenderer::SetRenderTargetAreaScissor","Invalid render area set"); break;
  }
  
}

void GLRenderer::SetViewPort(UINTVECTOR2 viLowerLeft, UINTVECTOR2 viUpperRight) {

  UINTVECTOR2 viSize = viUpperRight-viLowerLeft;

  float fAspect =(float)viSize.x/(float)viSize.y;
  float fFOVY  = 50.0f;
  float fZNear = 0.1f;
  float fZFar  = 100.0f;
  FLOATVECTOR3 vEye(0,0,1.6f), vAt(0,0,0), vUp(0,1,0);

  // viewport
  glViewport(viLowerLeft.x,viLowerLeft.y,viSize.x,viSize.y);

  if (m_bDoStereoRendering) {
    FLOATMATRIX4::BuildStereoLookAtAndProjection(vEye, vAt, vUp, fFOVY, fAspect, fZNear, fZFar, m_fStereoFocalLength, m_fStereoEyeDist, m_mView[0], m_mView[1], m_mProjection[0], m_mProjection[1]);
  } else {
    // view matrix
    m_mView[0].BuildLookAt(vEye, vAt, vUp);

    // projection matrix
    m_mProjection[0].Perspective(fFOVY,fAspect,fZNear,fZFar);
    m_mProjection[0].setProjection();
  }

  // forward the projection matrix to the culling object
  m_FrustumCullingLOD.SetProjectionMatrix(m_mProjection[0]);
  m_FrustumCullingLOD.SetScreenParams(fFOVY,fAspect,fZNear,fZFar,viSize.y);
}


void GLRenderer::RenderSlice(EWindowMode eDirection, UINT64 iSliceIndex, 
                             FLOATVECTOR3 vMinCoords, FLOATVECTOR3 vMaxCoords, 
                             UINT64VECTOR3 vDomainSize, DOUBLEVECTOR3 vAspectRatio,
                             DOUBLEVECTOR2 vWinAspectRatio) {

  switch (eDirection) {
    case WM_CORONAL : {
                          if (m_bFlipView[int(eDirection)].x) {
                              float fTemp = vMinCoords.x;
                              vMinCoords.x = vMaxCoords.x;
                              vMaxCoords.x = fTemp;
                          }

                          if (m_bFlipView[int(eDirection)].y) {
                              float fTemp = vMinCoords.z;
                              vMinCoords.z = vMaxCoords.z;
                              vMaxCoords.z = fTemp;
                          }

                          DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.xz()*DOUBLEVECTOR2(vWinAspectRatio);
                          v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
                          double fSliceIndex = double(iSliceIndex)/double(vDomainSize.y);
                          glBegin(GL_QUADS);
                            glTexCoord3d(vMinCoords.x,fSliceIndex,vMaxCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,fSliceIndex,vMaxCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,fSliceIndex,vMinCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMinCoords.x,fSliceIndex,vMinCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                          glEnd();
                          break;
                      }
    case WM_AXIAL : {
                          if (m_bFlipView[int(eDirection)].x) {
                              float fTemp = vMinCoords.x;
                              vMinCoords.x = vMaxCoords.x;
                              vMaxCoords.x = fTemp;
                          }

                          if (m_bFlipView[int(eDirection)].y) {
                              float fTemp = vMinCoords.y;
                              vMinCoords.y = vMaxCoords.y;
                              vMaxCoords.y = fTemp;
                          }

                          DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.xy()*DOUBLEVECTOR2(vWinAspectRatio);
                          v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
                          double fSliceIndex = double(iSliceIndex)/double(vDomainSize.z);
                          glBegin(GL_QUADS);
                            glTexCoord3d(vMinCoords.x,vMaxCoords.y,fSliceIndex);
                            glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,vMaxCoords.y,fSliceIndex);
                            glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,vMinCoords.y,fSliceIndex);
                            glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMinCoords.x,vMinCoords.y,fSliceIndex);
                            glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                          glEnd();
                          break;
                      }
    case WM_SAGITTAL : {
                          if (m_bFlipView[int(eDirection)].x) {
                              float fTemp = vMinCoords.y;
                              vMinCoords.y = vMaxCoords.y;
                              vMaxCoords.y = fTemp;
                          }

                          if (m_bFlipView[int(eDirection)].y) {
                              float fTemp = vMinCoords.z;
                              vMinCoords.z = vMaxCoords.z;
                              vMaxCoords.z = fTemp;
                          }

                          DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.yz()*DOUBLEVECTOR2(vWinAspectRatio);
                          v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
                          double fSliceIndex = double(iSliceIndex)/double(vDomainSize.x);
                          glBegin(GL_QUADS);
                            glTexCoord3d(fSliceIndex,vMinCoords.y,vMaxCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(fSliceIndex,vMaxCoords.y,vMaxCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(fSliceIndex,vMaxCoords.y,vMinCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(fSliceIndex,vMinCoords.y,vMinCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                          glEnd();
                          break;
                      }
    default        :  m_pMasterController->DebugOut()->Error("GLRenderer::RenderSlice","Invalid windowmode set"); break;
  }
}

bool GLRenderer::Render2DView(ERenderArea eREnderArea, EWindowMode eDirection, UINT64 iSliceIndex) {

  // bind offscreen buffer
  if (m_bUseMIP[size_t(eDirection)]) {
    m_pFBO3DImageCurrent[1]->Write();  // for MIP rendering "abuse" left-eye buffer for the itermediate results
  } else {
    m_pFBO3DImageCurrent[0]->Write();
  }

  SetDataDepShaderVars();
  
  // if we render a slice view or MIP preview
  if (!m_bUseMIP[size_t(eDirection)] || !m_bLODDisabled)  {
    
    if (!m_bUseMIP[size_t(eDirection)]) {
      switch (m_eRenderMode) {
        case RM_2DTRANS    :  m_p2DTransTex->Bind(1); 
                              m_pProgram2DTransSlice->Enable();
                              break;
        default            :  m_p1DTransTex->Bind(1); 
                              m_pProgram1DTransSlice->Enable();
                              break;
      }
      glDisable(GL_BLEND);
    } else {
      m_pProgramMIPSlice->Enable();
      glBlendFunc(GL_ONE, GL_ONE);
      glBlendEquation(GL_MAX);
      glEnable(GL_BLEND);

      SetRenderTargetAreaScissor(eREnderArea);
      glClearColor(0,0,0,0);
      glClear(GL_COLOR_BUFFER_BIT);
      glDisable( GL_SCISSOR_TEST );
    }

    glDisable(GL_DEPTH_TEST);

    UINT64 iCurrentLOD = 0;
    UINTVECTOR3 vVoxelCount;

    for (UINT64 i = 0;i<m_pDataset->GetInfo()->GetLODLevelCount();i++) {
      if (m_pDataset->GetInfo()->GetBrickCount(i).volume() == 1) {
          iCurrentLOD = i;
          vVoxelCount = UINTVECTOR3(m_pDataset->GetInfo()->GetDomainSize(i));
      }
    }

    if (!m_bUseMIP[size_t(eDirection)]) SetBrickDepShaderVarsSlice(vVoxelCount);

    // convert 3D variables to the more general ND scheme used in the memory manager, i.e. convert 3-vectors to stl vectors
    vector<UINT64> vLOD; vLOD.push_back(iCurrentLOD);
    vector<UINT64> vBrick; 
    vBrick.push_back(0);vBrick.push_back(0);vBrick.push_back(0);

    // get the 3D texture from the memory manager
    GLTexture3D* t = m_pMasterController->MemMan()->Get3DTexture(m_pDataset, vLOD, vBrick, m_bUseOnlyPowerOfTwo, 0, m_iFrameCounter);
    if(t!=NULL) t->Bind(0);

    // clear the target at the beginning
    SetRenderTargetAreaScissor(eREnderArea);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    FLOATVECTOR3 vMinCoords(0.5f/FLOATVECTOR3(vVoxelCount));
    FLOATVECTOR3 vMaxCoords(1.0f-vMinCoords);

    UINT64VECTOR3 vDomainSize = m_pDataset->GetInfo()->GetDomainSize();
    DOUBLEVECTOR3 vAspectRatio = m_pDataset->GetInfo()->GetScale() * DOUBLEVECTOR3(vDomainSize);  

    DOUBLEVECTOR2 vWinAspectRatio = 1.0 / DOUBLEVECTOR2(m_vWinSize);
    vWinAspectRatio = vWinAspectRatio / vWinAspectRatio.maxVal();

    if (!m_bUseMIP[size_t(eDirection)]) {
        RenderSlice(eDirection, iSliceIndex, vMinCoords, vMaxCoords, vDomainSize, vAspectRatio, vWinAspectRatio);
    } else {
      for (UINT64 i = 0;i<vDomainSize[size_t(eDirection)];i++) 
        RenderSlice(eDirection, i, vMinCoords, vMaxCoords, vDomainSize, vAspectRatio, vWinAspectRatio);
    }

    m_pMasterController->MemMan()->Release3DTexture(t);

    glEnable(GL_DEPTH_TEST);
    if (!m_bUseMIP[size_t(eDirection)]) {
      switch (m_eRenderMode) {
        case RM_2DTRANS    :  m_pProgram2DTransSlice->Disable(); break;
        default            :  m_pProgram1DTransSlice->Disable(); break;
      }
    }

    if (!m_bUseMIP[size_t(eDirection)]) {
      m_pProgramMIPSlice->Disable();
      m_pFBO3DImageCurrent[1]->FinishWrite();
    }    

  } else {
    if (m_bOrthoView) {
      FLOATMATRIX4 maOrtho;
      UINT64VECTOR3 vDomainSize = m_pDataset->GetInfo()->GetDomainSize();
      DOUBLEVECTOR2 vWinAspectRatio = 1.0 / DOUBLEVECTOR2(m_vWinSize);
      vWinAspectRatio = vWinAspectRatio / vWinAspectRatio.maxVal();
      float fRoot2Scale = (vWinAspectRatio.x < vWinAspectRatio.y) ? max(1.0,1.414213f * vWinAspectRatio.x/vWinAspectRatio.y) : 1.414213f;

      maOrtho.Ortho(-0.5*fRoot2Scale/vWinAspectRatio.x, +0.5*fRoot2Scale/vWinAspectRatio.x, 
                    -0.5*fRoot2Scale/vWinAspectRatio.y, +0.5*fRoot2Scale/vWinAspectRatio.y, 
                    -1.0, 1.0);
      maOrtho.setProjection();
    } 

    PlanHQMIPFrame();
    m_iFilledBuffers = 0;
    glClearColor(0,0,0,0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);


    RenderHQMIPPreLoop(eDirection);

    for (size_t iBrickIndex = 0;iBrickIndex<m_vCurrentBrickList.size();iBrickIndex++) {
      m_pMasterController->DebugOut()->Message("GLRenderer::Render2DView","Brick %i of %i", int(iBrickIndex+1),int(m_vCurrentBrickList.size()));

      // convert 3D variables to the more general ND scheme used in the memory manager, i.e. convert 3-vectors to stl vectors
      vector<UINT64> vLOD; vLOD.push_back(m_iCurrentLOD);
      vector<UINT64> vBrick; 
      vBrick.push_back(m_vCurrentBrickList[iBrickIndex].vCoords.x);
      vBrick.push_back(m_vCurrentBrickList[iBrickIndex].vCoords.y);
      vBrick.push_back(m_vCurrentBrickList[iBrickIndex].vCoords.z);

      // get the 3D texture from the memory manager
      GLTexture3D* t = m_pMasterController->MemMan()->Get3DTexture(m_pDataset, vLOD, vBrick, m_bUseOnlyPowerOfTwo, m_iIntraFrameCounter++, m_iFrameCounter);
      if(t) t->Bind(0);
      RenderHQMIPInLoop(m_vCurrentBrickList[iBrickIndex]);
      m_pMasterController->MemMan()->Release3DTexture(t);
    }
    RenderHQMIPPostLoop();
  }

  // apply 1D transferfunction to MIP image
  if (m_bUseMIP[size_t(eDirection)]) {
    glBlendEquation(GL_FUNC_ADD);
    glDisable( GL_BLEND );

    m_pFBO3DImageCurrent[1]->FinishWrite();
    m_pFBO3DImageCurrent[0]->Write();

    SetRenderTargetArea(RA_FULLSCREEN);
    SetRenderTargetAreaScissor(eREnderArea);

    m_pFBO3DImageCurrent[1]->Read(0);
    m_p1DTransTex->Bind(1);
    m_pProgramTransMIP->Enable();
    glDisable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);
      glTexCoord2d(0,0);
      glVertex3d(-1.0, -1.0, -0.5);
      glTexCoord2d(1,0);
      glVertex3d( 1.0, -1.0, -0.5);
      glTexCoord2d(1,1);
      glVertex3d( 1.0,  1.0, -0.5);
      glTexCoord2d(0,1);
      glVertex3d(-1.0,  1.0, -0.5);
    glEnd();
    glDisable( GL_SCISSOR_TEST );
    m_pFBO3DImageCurrent[1]->FinishRead(0);

    m_pProgramTransMIP->Disable();
  }

  m_pFBO3DImageCurrent[0]->FinishWrite();

  return true;
}

void GLRenderer::RenderHQMIPPreLoop(EWindowMode eDirection) {
  double dPI = 3.141592653589793238462643383;
  FLOATMATRIX4 matRotDir, matFlipX, matFlipY;
  switch (eDirection) {
    case WM_CORONAL : {
                        matRotDir.RotationX(-dPI/2.0);
                        break;
                      }
    case WM_AXIAL : {
                        break;
                      }
    case WM_SAGITTAL : {
                         FLOATMATRIX4 matTemp;
                         matRotDir.RotationX(-dPI/2.0);
                         matTemp.RotationY(-dPI/2.0);
                         matRotDir = matRotDir * matTemp;
                         break;
                      }
    default        :  m_pMasterController->DebugOut()->Error("GLRenderer::RenderHQMIPPreLoop","Invalid windowmode set"); break;
  }
  if (m_bFlipView[int(eDirection)].x) {
    matFlipY.Scaling(-1,1,1);
  }
  if (m_bFlipView[int(eDirection)].y) {
    matFlipX.Scaling(1,-1,1);
  }
  m_maMIPRotation.RotationY(dPI*double(m_fMIPRotationAngle)/180.0);
  m_maMIPRotation = matRotDir * matFlipX * matFlipY * m_maMIPRotation;
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor) {
  UINT64VECTOR3 vDomainSize = m_pDataset->GetInfo()->GetDomainSize();
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pDataset->GetInfo()->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();

  FLOATVECTOR3 vCenter(0,0,0);
  RenderBBox(vColor, vCenter, vExtend);
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor, const FLOATVECTOR3& vCenter, const FLOATVECTOR3& vExtend) {
  FLOATVECTOR3 vMinPoint, vMaxPoint;

  FLOATVECTOR3 vEExtend(vExtend+0.001f);

  vMinPoint = (vCenter - vEExtend/2.0);
  vMaxPoint = (vCenter + vEExtend/2.0);

  glBegin(GL_LINES);
    glColor4f(vColor.x,vColor.y,vColor.z,vColor.w);
    // FRONT
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);

    // BACK
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);

    // CONNECTION
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);
  glEnd();

}

void GLRenderer::NewFrameClear(ERenderArea eREnderArea) {
  m_iFilledBuffers = 0;
  SetRenderTargetAreaScissor(eREnderArea);

  glClearColor(0,0,0,0);

  m_pFBO3DImageCurrent[0]->Write();
  GLFBOTex::OneDrawBuffer();
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  m_pFBO3DImageCurrent[0]->FinishWrite();
  
  if (m_bDoStereoRendering) {
    m_pFBO3DImageCurrent[1]->Write();
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    m_pFBO3DImageCurrent[1]->FinishWrite();
  }

  glDisable( GL_SCISSOR_TEST ); // since we do not clear anymore in this subframe we do not need the scissor test, maybe disabling it saves performacnce
}

bool GLRenderer::Execute3DFrame(ERenderArea eREnderArea) {
  // are we starting a new LOD level?
  if (m_iBricksRenderedInThisSubFrame == 0) NewFrameClear(eREnderArea);
  
  // if zero bricks are to be rendered we have completed the draw job
  if (m_vCurrentBrickList.size() == 0) {
    m_pMasterController->DebugOut()->Message("GLRenderer::Execute3DFrame","zero bricks are to be rendered, completed the draw job");
    return true;
  }

  // if there is something left in the TODO list
  if (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame) {
    m_pMasterController->DebugOut()->Message("GLRenderer::Execute3DFrame","%i bricks left to render", int(UINT64(m_vCurrentBrickList.size())-m_iBricksRenderedInThisSubFrame));

    // setup shaders vars
    SetDataDepShaderVars(); 

    // Render a few bricks
    Render3DView();

    // if there is nothing left todo in this subframe -> present the result
    if (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
      m_pMasterController->DebugOut()->Message("GLRenderer::Execute3DFrame","Subframe completed.");
      return true;
    }
  } 
  return false;
}

void GLRenderer::RerenderPreviousResult(bool bTransferToFramebuffer) {
  // clear the framebuffer
  if (m_bClearFramebuffer) 
    ClearColorBuffer();
 
  if (bTransferToFramebuffer) {
    glViewport(0,0,m_vWinSize.x,m_vWinSize.y);
    m_iFilledBuffers++;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  } else {
    glDisable(GL_BLEND);
  }

  m_pFBO3DImageLast->Read(0);
  m_pFBO3DImageLast->ReadDepth(1);

  m_pProgramTrans->Enable();

  glDisable(GL_DEPTH_TEST);

  glBegin(GL_QUADS);
    glTexCoord2d(0,0);
    glVertex3d(-1.0, -1.0, -0.5);
    glTexCoord2d(1,0);
    glVertex3d( 1.0, -1.0, -0.5);
    glTexCoord2d(1,1);
    glVertex3d( 1.0,  1.0, -0.5);
    glTexCoord2d(0,1);
    glVertex3d(-1.0,  1.0, -0.5);
  glEnd();

  m_pProgramTrans->Disable();

  m_pFBO3DImageLast->FinishRead();
  m_pFBO3DImageLast->FinishDepthRead();
  glEnable(GL_DEPTH_TEST);
}


void GLRenderer::DrawLogo() {
  if (m_pLogoTex == NULL) return;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-0.5, +0.5, -0.5, +0.5, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  m_pLogoTex->Bind();
  glDisable(GL_TEXTURE_3D);
  glEnable(GL_TEXTURE_2D);

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  UINTVECTOR2 vSizes(m_pLogoTex->GetSize());
  FLOATVECTOR2 vTexelSize(1.0f/FLOATVECTOR2(vSizes));
  FLOATVECTOR2 vImageAspect(FLOATVECTOR2(vSizes)/FLOATVECTOR2(m_vWinSize));
  vImageAspect /= vImageAspect.maxVal();

  FLOATVECTOR2 vExtend(vImageAspect*0.25f);

  FLOATVECTOR2 vCenter;
  switch (m_iLogoPos) {
    case 0  : vCenter = FLOATVECTOR2(-0.50f+vExtend.x,  0.50f-vExtend.y); break;
    case 1  : vCenter = FLOATVECTOR2( 0.50f-vExtend.x,  0.50f-vExtend.y); break;
    case 2  : vCenter = FLOATVECTOR2(-0.50f+vExtend.x, -0.50f+vExtend.y); break;
    default : vCenter = FLOATVECTOR2( 0.50f-vExtend.x, -0.50f+vExtend.y); break;
  }

  glBegin(GL_QUADS);
    glColor4d(1,1,1,1);
    glTexCoord2d(0+vTexelSize.x,1-vTexelSize.y);
    glVertex3f(vCenter.x-vExtend.x, vCenter.y+vExtend.y, -0.5);
    glTexCoord2d(1-vTexelSize.x,1-vTexelSize.y);
    glVertex3f(vCenter.x+vExtend.x, vCenter.y+vExtend.y, -0.5);
    glTexCoord2d(1-vTexelSize.x,0+vTexelSize.y);
    glVertex3f(vCenter.x+vExtend.x, vCenter.y-vExtend.y, -0.5);
    glTexCoord2d(0+vTexelSize.x,0+vTexelSize.y);
    glVertex3f(vCenter.x-vExtend.x, vCenter.y-vExtend.y, -0.5);
  glEnd();

  glDisable(GL_TEXTURE_2D);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

void GLRenderer::DrawBackGradient() {
  glDisable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-1.0, +1.0, +1.0, -1.0, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDisable(GL_TEXTURE_3D);
  glDisable(GL_TEXTURE_2D);

  glBegin(GL_QUADS);
    glColor4d(m_vBackgroundColors[0].x,m_vBackgroundColors[0].y,m_vBackgroundColors[0].z,1);
    glVertex3d(-1.0, -1.0, -0.5);
    glVertex3d( 1.0, -1.0, -0.5);
    glColor4d(m_vBackgroundColors[1].x,m_vBackgroundColors[1].y,m_vBackgroundColors[1].z,1);
    glVertex3d( 1.0,  1.0, -0.5);
    glVertex3d(-1.0,  1.0, -0.5);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glEnable(GL_DEPTH_TEST);
}

void GLRenderer::Cleanup() {
  if (m_pFBO3DImageLast)      {m_pMasterController->MemMan()->FreeFBO(m_pFBO3DImageLast); m_pFBO3DImageLast =NULL;}

  for (UINT32 i = 0;i<2;i++) {
    if (m_pFBO3DImageCurrent[i])   {m_pMasterController->MemMan()->FreeFBO(m_pFBO3DImageCurrent[i]); m_pFBO3DImageCurrent[i] = NULL;}
    if (m_pFBOIsoHit[i])           {m_pMasterController->MemMan()->FreeFBO(m_pFBOIsoHit[i]);m_pFBOIsoHit[i] = NULL;}
    if (m_pFBOCVHit[i])            {m_pMasterController->MemMan()->FreeFBO(m_pFBOCVHit[i]);m_pFBOCVHit[i] = NULL;}
  }

  if (m_pProgramTrans)        {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramTrans); m_pProgramTrans =NULL;}
  if (m_pProgram1DTransSlice) {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTransSlice); m_pProgram1DTransSlice =NULL;}
  if (m_pProgram2DTransSlice) {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTransSlice); m_pProgram2DTransSlice =NULL;}
  if (m_pProgramMIPSlice)     {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramMIPSlice); m_pProgramMIPSlice =NULL;}
  if (m_pProgramHQMIPRot)     {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramHQMIPRot); m_pProgramHQMIPRot =NULL;}
  if (m_pProgramTransMIP)     {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramTransMIP); m_pProgramTransMIP =NULL;}
  if (m_pProgram1DTrans[0])   {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTrans[0]); m_pProgram1DTrans[0] =NULL;}
  if (m_pProgram1DTrans[1])   {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTrans[1]); m_pProgram1DTrans[1] =NULL;}
  if (m_pProgram2DTrans[0])   {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTrans[0]); m_pProgram2DTrans[0] =NULL;}
  if (m_pProgram2DTrans[1])   {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTrans[1]); m_pProgram2DTrans[1] =NULL;}
  if (m_pProgramIso)          {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramIso); m_pProgramIso =NULL;}
  if (m_pProgramIsoCompose)   {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramIsoCompose); m_pProgramIsoCompose = NULL;}
  if (m_pProgramCVCompose)    {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramCVCompose); m_pProgramCVCompose = NULL;}
  if (m_pProgramComposeAnaglyphs){m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramComposeAnaglyphs); m_pProgramComposeAnaglyphs = NULL;}

  if (m_pLogoTex)             {m_pMasterController->MemMan()->FreeTexture(m_pLogoTex); m_pLogoTex =NULL;}
}

void GLRenderer::CreateOffscreenBuffers() {
  if (m_pFBO3DImageLast)      {m_pMasterController->MemMan()->FreeFBO(m_pFBO3DImageLast); m_pFBO3DImageLast =NULL;}
  for (UINT32 i = 0;i<2;i++) {
    if (m_pFBO3DImageCurrent[i])   {m_pMasterController->MemMan()->FreeFBO(m_pFBO3DImageCurrent[i]); m_pFBO3DImageCurrent[i] = NULL;}
    if (m_pFBOIsoHit[i])           {m_pMasterController->MemMan()->FreeFBO(m_pFBOIsoHit[i]);m_pFBOIsoHit[i] = NULL;}
    if (m_pFBOCVHit[i])            {m_pMasterController->MemMan()->FreeFBO(m_pFBOCVHit[i]);m_pFBOCVHit[i] = NULL;}
  }

  if (m_vWinSize.area() > 0) {
    for (UINT32 i = 0;i<2;i++) {
      switch (m_eBlendPrecision) {
        case BP_8BIT  : if (i==0) m_pFBO3DImageLast = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA8, 8*4, true);
                        m_pFBO3DImageCurrent[i] = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA8, 8*4, true);
                        break;
        case BP_16BIT : if (i==0)m_pFBO3DImageLast = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 16*4, true);
                        m_pFBO3DImageCurrent[i] = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 16*4, true);
                        break;
        case BP_32BIT : if (i==0)m_pFBO3DImageLast = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA32F_ARB, 32*4, true);
                        m_pFBO3DImageCurrent[i] = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA32F_ARB, 32*4, true);
                        break;
        default       : m_pMasterController->DebugOut()->Message("GLRenderer::CreateOffscreenBuffer","Invalid Blending Precision");
                        if (i==0) m_pFBO3DImageLast = NULL; 
                        m_pFBO3DImageCurrent[i] = NULL;
                        break;
      }
      m_pFBOIsoHit[i]   = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 16*4, true, 2);
      m_pFBOCVHit[i]    = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 16*4, true, 2);
    }
  }
}

void GLRenderer::SetBrickDepShaderVarsSlice(const UINTVECTOR3& vVoxelCount) {
  if (m_eRenderMode ==  RM_2DTRANS) {
    FLOATVECTOR3 vStep = 1.0f/FLOATVECTOR3(vVoxelCount);
    m_pProgram2DTransSlice->SetUniformVector("vVoxelStepsize", vStep.x, vStep.y, vStep.z);
  }
}

void GLRenderer::SetDataDepShaderVars() {
  m_pMasterController->DebugOut()->Message("GLRenderer::SetDataDepShaderVars","Setting up vars");

  size_t iMaxValue     = m_p1DTrans->GetSize();
  UINT32 iMaxRange     = UINT32(1<<m_pDataset->GetInfo()->GetBitwith());
  float fScale         = float(iMaxRange)/float(iMaxValue);
  float fGradientScale = 1.0f/m_pDataset->GetMaxGradMagnitude();

  m_pProgramTransMIP->Enable();
  m_pProgramTransMIP->SetUniformVector("fTransScale",fScale);
  m_pProgramTransMIP->Disable();

  switch (m_eRenderMode) {
    case RM_1DTRANS    :  {
                            m_pProgram1DTransSlice->Enable();
                            m_pProgram1DTransSlice->SetUniformVector("fTransScale",fScale);
                            m_pProgram1DTransSlice->Disable();

                            m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Enable();
                            m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("fTransScale",fScale);
                            m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Disable();
                            break;
                          }
    case RM_2DTRANS    :  {
                            m_pProgram2DTransSlice->Enable();
                            m_pProgram2DTransSlice->SetUniformVector("fTransScale",fScale);
                            m_pProgram2DTransSlice->SetUniformVector("fGradientScale",fGradientScale);
                            m_pProgram2DTransSlice->Disable();

                            m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Enable();
                            m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("fTransScale",fScale);
                            m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("fGradientScale",fGradientScale);
                            m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Disable();
                            break;
                          }
    case RM_ISOSURFACE : {
                            // as we are rendering the 2 slices with the 1d transferfunction in iso mode update that shader also
                            m_pProgram1DTransSlice->Enable();
                            m_pProgram1DTransSlice->SetUniformVector("fTransScale",fScale);
                            m_pProgram1DTransSlice->Disable();

                            m_pProgramIso->Enable();
                            m_pProgramIso->SetUniformVector("fIsoval",m_fScaledIsovalue);
                            m_pProgramIso->Disable();
                            break;
                          }
    case RM_INVALID    :  m_pMasterController->DebugOut()->Error("GLRenderer::SetDataDepShaderVars","Invalid rendermode set");
                          break;
  }

  m_pMasterController->DebugOut()->Message("GLRenderer::SetDataDepShaderVars","Done");
}


void GLRenderer::SetBlendPrecision(EBlendPrecision eBlendPrecision) {
  if (eBlendPrecision != m_eBlendPrecision) {
    AbstrRenderer::SetBlendPrecision(eBlendPrecision);
    CreateOffscreenBuffers();
  }
}

bool GLRenderer::LoadAndVerifyShader(string strVSFile, string strFSFile, const std::vector<std::string>& strDirs, GLSLProgram** pShaderProgram) {
  for (size_t i = 0;i<strDirs.size();i++) {
    string strCompleteVSFile = strDirs[i] + "/" + strVSFile;
    string strCompleteFSFile = strDirs[i] + "/" + strFSFile;

    if (LoadAndVerifyShader(strCompleteVSFile, strCompleteFSFile, pShaderProgram, false)) return true;
  }

  // if all else fails probe current directory and all of its subdirectories
  if (LoadAndVerifyShader(strVSFile, strFSFile, pShaderProgram, true)) 
    return true;
  else
    return false;
}

bool GLRenderer::LoadAndVerifyShader(string strVSFile, string strFSFile, GLSLProgram** pShaderProgram, bool bSearchSubdirs) {

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
  if (SysTools::FileExists(SysTools::GetFromResourceOnMac(strVSFile))) strVSFile = SysTools::GetFromResourceOnMac(strVSFile);
  if (SysTools::FileExists(SysTools::GetFromResourceOnMac(strFSFile))) strFSFile = SysTools::GetFromResourceOnMac(strFSFile);
#endif

  string strActualVSFile = "";
  if (!SysTools::FileExists(strVSFile) && bSearchSubdirs) {
    // if vertex shader is not found in the given directory, probe all subdirectories
    vector<string> subdirs = SysTools::GetSubDirList("");
    subdirs.push_back(".");

    string strDirlessVSFile = SysTools::GetFilename(strVSFile);
    for (size_t i = 0;i<subdirs.size();i++) {
      string strTestVSFile = subdirs[i] + "/" + strDirlessVSFile;

      if (SysTools::FileExists(strTestVSFile)) {
        strActualVSFile = strTestVSFile;
        break;
      }
    }

    if (strActualVSFile == "") {
      m_pMasterController->DebugOut()->Error("GLRenderer::LoadAndVerifyShader","Unable to locate vertex shader %s (%s)",strDirlessVSFile.c_str(), strVSFile.c_str());
      return false;
    } else
      m_pMasterController->DebugOut()->Message("GLRenderer::LoadAndVerifyShader","Changed vertex shader %s to %s",strVSFile.c_str(), strActualVSFile.c_str());

  } else {
    strActualVSFile = strVSFile;
  }

  string strActualFSFile = "";
  if (!SysTools::FileExists(strFSFile) && bSearchSubdirs) {
    // if fragment shader is not found in the given directory, probe all subdirectories
    vector<string> subdirs = SysTools::GetSubDirList("");
    subdirs.push_back(".");

    string strDirlessFSFile = SysTools::GetFilename(strFSFile);
    for (size_t i = 0;i<subdirs.size();i++) {
      string strTestFSFile = subdirs[i] + "/" + strDirlessFSFile;

      if (SysTools::FileExists(strTestFSFile)) {
        strActualFSFile = strTestFSFile;
        break;
      }
    }

    if (strActualFSFile == "") {
      m_pMasterController->DebugOut()->Error("GLRenderer::LoadAndVerifyShader","Unable to locate fragment shader %s (%s)",strDirlessFSFile.c_str(), strFSFile.c_str());
      return false;
    } else
      m_pMasterController->DebugOut()->Message("GLRenderer::LoadAndVerifyShader","Changed fragment shader %s to %s",strFSFile.c_str(), strActualFSFile.c_str());

  } else {
    strActualFSFile = strFSFile;
  }


  if (SysTools::FileExists(strActualVSFile) && SysTools::FileExists(strActualFSFile)) {
    (*pShaderProgram) = m_pMasterController->MemMan()->GetGLSLProgram(strActualVSFile, strActualFSFile);

    if ((*pShaderProgram) == NULL || !(*pShaderProgram)->IsValid()) {
        m_pMasterController->DebugOut()->Error("GLRenderer::LoadAndVerifyShader","Error loading a shader combination VS %s and FS %s.", strActualVSFile.c_str(), strActualFSFile.c_str());
        m_pMasterController->MemMan()->FreeGLSLProgram(*pShaderProgram);
        return false;
    } else return true;

  } else {
    (*pShaderProgram) = NULL;
    return false;
  }

}


void GLRenderer::BBoxPreRender() {
  // for rendering modes other than isosurface render the bbox in the first pass once to init the depth buffer
  // for isosurface rendering we can go ahead and render the bbox directly as isosurfacing 
  // writes out correct depth values
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView || m_bAvoidSeperateCompositing) {
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    if (m_bRenderGlobalBBox) 
      RenderBBox();
    if (m_bRenderLocalBBox) {
      for (UINT64 iCurrentBrick = 0;iCurrentBrick<m_vCurrentBrickList.size();iCurrentBrick++) {
        RenderBBox(FLOATVECTOR4(0,1,0,1), m_vCurrentBrickList[iCurrentBrick].vCenter, m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  } else {
    glDisable(GL_BLEND);
    if (m_bRenderGlobalBBox) RenderBBox();
    if (m_bRenderLocalBBox) {
      for (UINT64 iCurrentBrick = 0;iCurrentBrick<m_vCurrentBrickList.size();iCurrentBrick++) {
        RenderBBox(FLOATVECTOR4(0,1,0,1), m_vCurrentBrickList[iCurrentBrick].vCenter, m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }
  }
}

void GLRenderer::BBoxPostRender() {
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView || m_bAvoidSeperateCompositing) {
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    if (m_bRenderGlobalBBox) RenderBBox();
    if (m_bRenderLocalBBox) {
      for (UINT64 iCurrentBrick = 0;iCurrentBrick<m_vCurrentBrickList.size();iCurrentBrick++) {
        RenderBBox(FLOATVECTOR4(0,1,0,1), m_vCurrentBrickList[iCurrentBrick].vCenter, m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
  }
}

bool GLRenderer::LoadDataset(const string& strFilename) {
  if (AbstrRenderer::LoadDataset(strFilename)) {
    if (m_pProgram1DTrans[0] != NULL) SetDataDepShaderVars();
    return true;
  } else return false;
}

void GLRenderer::Recompose3DView(ERenderArea eArea) {
  m_pMasterController->DebugOut()->Message("GLRenderer::Recompose3DView","Recomposing...");
  
  NewFrameClear(eArea);

  m_pFBO3DImageCurrent[0]->Write();
  GLFBOTex::OneDrawBuffer();
  m_mProjection[0].setProjection();
  m_matModelView[0].setModelview();
  BBoxPreRender();
  Render3DPreLoop();
  Render3DPostLoop();
  ComposeSurfaceImage(0);
  BBoxPostRender();
  m_pFBO3DImageCurrent[0]->FinishWrite();

  if (m_bDoStereoRendering) {
    m_pFBO3DImageCurrent[1]->Write();
    m_mProjection[1].setProjection();
    m_matModelView[1].setModelview();
    BBoxPreRender();
    Render3DPreLoop();
    Render3DPostLoop();
    ComposeSurfaceImage(1);
    BBoxPostRender();
    m_pFBO3DImageCurrent[1]->FinishWrite();
  }
}

void GLRenderer::Render3DView() {  
  // in the first frame of a new lod level write the bounding boxes into depthbuffer (and for isosurfacing also into colorbuffer)
  if (m_iBricksRenderedInThisSubFrame == 0) {
    m_pFBO3DImageCurrent[0]->Write();
    m_mProjection[0].setProjection();
    m_matModelView[0].setModelview();
    BBoxPreRender();
    m_pFBO3DImageCurrent[0]->FinishWrite();  
    if (m_bDoStereoRendering) {
      m_pFBO3DImageCurrent[1]->Write();
      m_mProjection[1].setProjection();
      m_matModelView[1].setModelview();
      BBoxPreRender();
      m_pFBO3DImageCurrent[1]->FinishWrite();
    }
  }
  Render3DPreLoop();

  // loop over all bricks in the current LOD level
  clock_t timeStart, timeProbe;
  timeStart = timeProbe = clock();

  while (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame && float(timeProbe-timeStart)*1000.0f/float(CLOCKS_PER_SEC) < m_iTimeSliceMSecs) {
    m_pMasterController->DebugOut()->Message("GLRenderer::Render3DView","  Brick %i of %i", int(m_iBricksRenderedInThisSubFrame+1),int(m_vCurrentBrickList.size()));

    // convert 3D variables to the more general ND scheme used in the memory manager, e.i. convert 3-vectors to stl vectors
    vector<UINT64> vLOD; vLOD.push_back(m_iCurrentLOD);
    vector<UINT64> vBrick; 
    vBrick.push_back(m_vCurrentBrickList[m_iBricksRenderedInThisSubFrame].vCoords.x);
    vBrick.push_back(m_vCurrentBrickList[m_iBricksRenderedInThisSubFrame].vCoords.y);
    vBrick.push_back(m_vCurrentBrickList[m_iBricksRenderedInThisSubFrame].vCoords.z);

    // get the 3D texture from the memory manager
    GLTexture3D* t = m_pMasterController->MemMan()->Get3DTexture(m_pDataset, vLOD, vBrick, m_bUseOnlyPowerOfTwo, m_iIntraFrameCounter++, m_iFrameCounter);
    if(t) t->Bind(0);

    Render3DInLoop(m_iBricksRenderedInThisSubFrame,0);
    if (m_bDoStereoRendering) {
     
      if (m_vLeftEyeBrickList[m_iBricksRenderedInThisSubFrame].vCoords != m_vCurrentBrickList[m_iBricksRenderedInThisSubFrame].vCoords) {
        vBrick.clear();
        vBrick.push_back(m_vLeftEyeBrickList[m_iBricksRenderedInThisSubFrame].vCoords.x);
        vBrick.push_back(m_vLeftEyeBrickList[m_iBricksRenderedInThisSubFrame].vCoords.y);
        vBrick.push_back(m_vLeftEyeBrickList[m_iBricksRenderedInThisSubFrame].vCoords.z);

        m_pMasterController->MemMan()->Release3DTexture(t);
        t = m_pMasterController->MemMan()->Get3DTexture(m_pDataset, vLOD, vBrick, m_bUseOnlyPowerOfTwo, m_iIntraFrameCounter++, m_iFrameCounter);
        if(t) t->Bind(0);
      }
      Render3DInLoop(m_iBricksRenderedInThisSubFrame,1);
    }

    // release the 3D texture
    m_pMasterController->MemMan()->Release3DTexture(t);

    // count the bricks rendered
    m_iBricksRenderedInThisSubFrame++;
    
    // time this loop
    if (!m_bLODDisabled) timeProbe = clock();
  }

  Render3DPostLoop();

  if (m_eRenderMode == RM_ISOSURFACE && m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {    
    m_pFBO3DImageCurrent[0]->Write();
    GLFBOTex::OneDrawBuffer();
    ComposeSurfaceImage(0);
    m_pFBO3DImageCurrent[0]->FinishWrite();
    if (m_bDoStereoRendering) {
      m_pFBO3DImageCurrent[1]->Write();
      ComposeSurfaceImage(1);
      m_pFBO3DImageCurrent[1]->FinishWrite();
    }
  }

  // at the very end render the bboxes
  if (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
    m_pFBO3DImageCurrent[0]->Write();
    m_mProjection[0].setProjection();
    m_matModelView[0].setModelview();
    BBoxPostRender();
    m_pFBO3DImageCurrent[0]->FinishWrite();  
    if (m_bDoStereoRendering) {
      m_pFBO3DImageCurrent[1]->Write();
      m_mProjection[1].setProjection();
      m_matModelView[1].setModelview();
      BBoxPostRender();
      m_pFBO3DImageCurrent[1]->FinishWrite();
    }
  }    
}

void GLRenderer::SetLogoParams(std::string strLogoFilename, int iLogoPos) {
  AbstrRenderer::SetLogoParams(strLogoFilename, iLogoPos);

  if (m_pLogoTex) {m_pMasterController->MemMan()->FreeTexture(m_pLogoTex); m_pLogoTex =NULL;}
  if (m_strLogoFilename != "")
  m_pLogoTex = m_pMasterController->MemMan()->Load2DTextureFromFile(m_strLogoFilename);
  ScheduleWindowRedraw(WM_3D);
}

void GLRenderer::ComposeSurfaceImage(int iStereoID) {
  glEnable(GL_DEPTH_TEST);

  m_pFBOIsoHit[iStereoID]->Read(0, 0);
  m_pFBOIsoHit[iStereoID]->Read(1, 1);

  if (m_bDoClearView) {
    m_pProgramCVCompose->Enable(); 
    m_pProgramCVCompose->SetUniformVector("vLightDiffuse",m_vIsoColor.x, m_vIsoColor.y, m_vIsoColor.z);
    m_pProgramCVCompose->SetUniformVector("vLightDiffuse2",m_vCVColor.x, m_vCVColor.y, m_vCVColor.z);
    m_pProgramCVCompose->SetUniformVector("vCVParam",m_fCVSize, m_fCVContextScale, m_fCVBorderScale);
    m_pProgramCVCompose->SetUniformVector("vCVPickPos", m_vCVPos.x, m_vCVPos.y);
    m_pFBOCVHit[iStereoID]->Read(2, 0);
    m_pFBOCVHit[iStereoID]->Read(3, 1);
    glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  } else {
    m_pProgramIsoCompose->Enable();
    m_pProgramIsoCompose->SetUniformVector("vLightDiffuse",m_vIsoColor.x, m_vIsoColor.y, m_vIsoColor.z);
  }

  glBegin(GL_QUADS);
    glTexCoord2d(0,1);
    glVertex3d(-1.0,  1.0, -0.5);
    glTexCoord2d(1,1);
    glVertex3d( 1.0,  1.0, -0.5);
    glTexCoord2d(1,0);
    glVertex3d( 1.0, -1.0, -0.5);
    glTexCoord2d(0,0);
    glVertex3d(-1.0, -1.0, -0.5);
  glEnd();

  if (m_bDoClearView) {
    m_pFBOCVHit[iStereoID]->FinishRead(0);
    m_pFBOCVHit[iStereoID]->FinishRead(1);
    m_pProgramCVCompose->Disable();
  } else m_pProgramIsoCompose->Disable();

  m_pFBOIsoHit[iStereoID]->FinishRead(1);
  m_pFBOIsoHit[iStereoID]->FinishRead(0);

  m_bPerformReCompose = false;
}
