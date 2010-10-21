#include "stdafx.h"
#include <windows.h>
#include <deque>
#include <celsus/graphics.hpp>
#include <celsus/dynamic_vb.hpp>
#include <celsus/vertex_types.hpp>
#include <celsus/error2.hpp>
#include <celsus/Logger.hpp>
#include <celsus/effect_wrapper.hpp>
#include <D3DX10math.h>

const float kPi = (float)D3DX_PI;

char shader[] = " "\
"float4 scale; 											  "\
"struct psInput												"\
"{																		"\
"	float4 pos : SV_Position;						"\
"	float4 col : Color;									"\
"};																		"\
"																			"\
"struct vsInput												"\
"{																		"\
"	float4 pos : SV_Position;						"\
"	float4 col : Color;									"\
"};																		"\
"psInput vsMain(in vsInput v)					"\
"{																		"\
"	psInput o = (psInput)0;							"\
"	o.pos.x = v.pos.x * scale.x;			  "\
"	o.pos.y = v.pos.y * scale.y;			  "\
"	o.col = v.col;											"\
"	return o;														"\
"}																		"\
"																			"\
"float4 psMain(in psInput v) : SV_Target	"\
"{"\
"	return v.col;"\
"}";

// Thud - 2d renderer
struct Thud
{
	Thud();

  static Thud& instance();

  bool init();
  bool close();

  void push_state();
  void pop_state();

	void add_canvas();

  void set_extents(const D3DXVECTOR2& extents);

  void set_fill(const D3DXCOLOR& col);
  void set_stroke(const D3DXCOLOR& col);

  void clear(const D3DXCOLOR& col);

  void set_circle_segments(int num_segments);
  void circle(const D3DXVECTOR3& o, float r);
  void circle(const D3DXVECTOR3& o, float r, int segments);

  void rect(const D3DXVECTOR3& top_left, const D3DXVECTOR3& size);

  void start_frame();
  void render();

  struct State
  {
    State()
      : circle_segments(20)
      , fill(D3DXCOLOR(0,0,0,0))
      , stroke(D3DXCOLOR(1,1,1,1))
    {
    }
    int circle_segments;
    D3DXCOLOR fill;
    D3DXCOLOR stroke;
  };

	struct Canvas
	{
		Canvas()
			: extents(2,2)
      , scale(1,1)
      , ptr(nullptr)
		{
		}

		bool init()
		{
			RETURN_ON_FAIL_BOOL_E(verts.create(32 * 1024));
			return true;
		}

    void map()
    {
      ptr = verts.map();
    }

    int unmap()
    {
      const int c = verts.unmap(ptr);
      ptr = nullptr;
      return c;
    }

    D3DXVECTOR2 scale;
		D3DXVECTOR2 extents;
		DynamicVb<PosCol> verts;
    PosCol *ptr;
	};

	EffectWrapper *_effect;
	CComPtr<ID3D11InputLayout> _layout;
  CComPtr<ID3D11Buffer> _cbuffer;
  std::deque<State> _state_stack;
	std::deque<Canvas> _canvas_stack;
  static Thud *_instance;
};

Thud *Thud::_instance = nullptr;

Thud::Thud()
	: _effect(nullptr)
{

}

Thud& Thud::instance()
{
  return !_instance ? *(_instance = new Thud) : *_instance;
}

bool Thud::init()
{
  // default state
  _state_stack.push_back(State());
	_canvas_stack.push_back(Canvas());
	RETURN_ON_FAIL_BOOL_E(_canvas_stack.back().init());
	
	_effect = new EffectWrapper();
	RETURN_ON_FAIL_BOOL_E(_effect->load_shaders(shader, sizeof(shader), "vsMain", NULL, "psMain"));

	RETURN_ON_FAIL_BOOL_E(InputDesc().
		add("SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0).
		add("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0).
		create(_layout, _effect));

  RETURN_ON_FAIL_BOOL_E(create_cbuffer(Graphics::instance().device(), 1 * sizeof(D3DXVECTOR4), &_cbuffer.p));

  return true;
}

bool Thud::close()
{
	SAFE_DELETE(_effect);

  delete this;
  _instance = nullptr;
  return true;
}

void Thud::set_extents(const D3DXVECTOR2& extents)
{
  Canvas& cur = _canvas_stack.back();
  cur.extents = extents;
  cur.scale.x = 2 / extents.x;
  cur.scale.y = 2 / extents.y;
}

void Thud::set_fill(const D3DXCOLOR& col)
{
  _state_stack.back().fill = col;
}

void Thud::set_stroke(const D3DXCOLOR& col)
{
  _state_stack.back().stroke = col;
}

void Thud::clear(const D3DXCOLOR& col)
{

}

void Thud::start_frame()
{
  Canvas& canvas = _canvas_stack.back();
  canvas.map();

  Graphics& graphics = Graphics::instance();
  ID3D11Device* device = graphics.device();
  ID3D11DeviceContext* context = graphics.context();

  // set the cbuffer
  D3DXVECTOR4 *scale = (D3DXVECTOR4 *)map_buffer(context, _cbuffer);
  scale->x = canvas.scale.x;
  scale->y = canvas.scale.y;
  unmap_buffer(context, _cbuffer);
  context->VSSetConstantBuffers(0, 1, &_cbuffer.p);
}

void Thud::render()
{
  Graphics& graphics = Graphics::instance();
  ID3D11Device* device = graphics.device();
  ID3D11DeviceContext* context = graphics.context();

  Canvas& canvas = _canvas_stack.back();
  const int num_verts = canvas.unmap();

  context->OMSetDepthStencilState(graphics.default_dss(), graphics.default_stencil_ref());
  context->OMSetBlendState(graphics.default_blend_state(), graphics.default_blend_factors(), graphics.default_sample_mask());

  _effect->set_shaders(context);
  context->IASetInputLayout(_layout);
  set_vb(context, canvas.verts.vb(), canvas.verts.stride);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->Draw(num_verts, 0);
}

void Thud::circle(const D3DXVECTOR3& o, float r)
{
  const State& state = _state_stack.back();
  Canvas& canvas = _canvas_stack.back();
  PosCol*& ptr = canvas.ptr;
  const float inc = 2 * kPi / (state.circle_segments);
  float ofs = 0;
  D3DXVECTOR3 cur = D3DXVECTOR3(o.x + r * cosf(ofs), o.y + r * sinf(ofs), o.z);
  for (int i = 0; i < state.circle_segments; ++i) {
    D3DXVECTOR3 next = D3DXVECTOR3(o.x + r * cosf(ofs + inc), o.y + r * sinf(ofs + inc), o.z);
    *ptr++ = PosCol(o, state.fill);
    *ptr++ = PosCol(next, state.fill);
    *ptr++ = PosCol(cur, state.fill);
    cur = next;
    ofs += inc;
  }
}

void Thud::rect(const D3DXVECTOR3& top_left, const D3DXVECTOR3& size)
{

}

void Thud::push_state()
{
  _state_stack.push_back(State());
}

void Thud::pop_state()
{
  _state_stack.pop_back();   
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch(message)
  {
  case WM_KEYUP:
    switch (wParam)
    {
    case VK_ESCAPE:
      PostQuitMessage(0);
      break;
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }

  return 0;
}

int WINAPI WinMain( __in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPSTR lpCmdLine, __in int nShowCmd )
{

  static TCHAR window_class[] = _T("Thud Main Window");
  const int width = 800;
  const int height = 600;

  WNDCLASSEX wcex;
  ZeroMemory(&wcex, sizeof(wcex));
  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = hInstance;
  wcex.lpszClassName = window_class;

  if (!RegisterClassEx(&wcex))
    return 1;

  HWND hwnd = CreateWindow(window_class, window_class, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);
  if (!hwnd)
    return 1;

  ShowWindow(hwnd, nShowCmd);
  UpdateWindow(hwnd);

  Graphics& graphics = Graphics::instance();
  Thud& thud = Thud::instance();

  if (!graphics.init_directx(hwnd, width, height))
    return 1;

  if (!thud.init())
    return 1;

  thud.set_extents(D3DXVECTOR2(10, 10));

  MSG msg;
  ZeroMemory(&msg, sizeof(msg));
  while (WM_QUIT != msg.message) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      graphics.clear(D3DXCOLOR(0.5f, 0.5f, 0.5f, 1));
      thud.start_frame();
      thud.clear(D3DXCOLOR(0, 1, 0, 1));
      thud.set_fill(D3DXCOLOR(1,1,1,1));
      thud.circle(D3DXVECTOR3(0,0,0.5f), 0.015f);
      thud.render();
      graphics.present();
    }
  }

  thud.close();
  graphics.close();

  return (int)msg.wParam;
}
