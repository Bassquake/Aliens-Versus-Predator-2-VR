//-----------------------------------------------------------------------------
//
//	File:	D3DStateChange.h
//	Author:	Jim Edwards
//	Date:	5/1/00
//	Desc:	Object to set the rendering states and modes for a given set of 
//			polies. Checks the current modes to determine if a change is needed.
//

#ifndef D3D_STATE_CHANGE
#define D3D_STATE_CHANGE

#include <d3dTypes.h>
#include <vector>

// RenderState defines a requested state change
struct RenderState
{
	D3DRENDERSTATETYPE	m_RenderStateType;
	DWORD				m_RenderState;

	RenderState()	:	m_RenderStateType(static_cast<D3DRENDERSTATETYPE>(0)),
						m_RenderState(0) {;}

	RenderState(	const D3DRENDERSTATETYPE renderStateType,
					const DWORD renderState)
					:	m_RenderStateType(renderStateType),
						m_RenderState(renderState) {;}

	void SetState(	const D3DRENDERSTATETYPE renderStateType,
					const DWORD renderState)
					{
						m_RenderStateType	= renderStateType;
						m_RenderState		= renderState;
					}
};

// TextureState defines a requested texture or texture state change
struct TextureState
{
	DWORD						m_Stage;
	D3DTEXTURESTAGESTATETYPE	m_TextureStateType;
	DWORD						m_TextureState;

	TextureState()	:	m_Stage(0),
						m_TextureStateType(static_cast<D3DTEXTURESTAGESTATETYPE>(0)),
						m_TextureState(0) {;}

	TextureState(	const DWORD stage, 
					const D3DTEXTURESTAGESTATETYPE textureStateType, 
					const DWORD textureState)
					:	m_Stage(stage), 
						m_TextureStateType(textureStateType),
						m_TextureState(textureState) {;}

	void SetState(	const DWORD stage,
					const D3DTEXTURESTAGESTATETYPE textureStateType,
					const DWORD textureState)
					{
						m_Stage = stage;
						m_TextureStateType = textureStateType;
						m_TextureState = textureState;
					}
};

struct StateChange
{
	typedef std::vector<RenderState>	RenderContainer;
	typedef std::vector<TextureState>	TextureContainer;

	RenderContainer		m_RenderList;
	TextureContainer	m_TextureList;

public:
	StateChange() {;}
	StateChange(const RenderState& render) : m_RenderList(1, render) {;}
	StateChange(const TextureState& texture) : m_TextureList(1, texture) {;}

	void Add(const RenderState& render) {m_RenderList.push_back(render);}
	void Add(const TextureState& texture) {m_TextureList.push_back(texture);}

	void Erase() {EraseRender();EraseTexture();}
	void EraseTexture() {m_TextureList.clear();}
	void EraseRender() {m_RenderList.clear();}
};

#endif		// D3D_STATE_CHANGE
