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

// 2d renderer
struct Vec2
{
  Vec2(float x, float y) : x(x), y(0) {}
  Vec2() {}
  union {
    struct {float x;  float y; };
    float d[2];
  };
};

char shader[] = " "\
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
"																			"\
"psInput vsMain(in vsInput v)					"\
"{																		"\
"	psInput o = (psInput)0;							"\
"	o.pos = v.pos;											"\
"	o.col = v.col;											"\
"	return o;														"\
"}																		"\
"																			"\
"float4 psMain(in psInput v) : SV_Target	"\
"{"\
"	return v.col;"\
"}";

struct Thud
{
	Thud();

  static Thud& instance();

  bool init();
  bool close();

  void push_state();
  void pop_state();

	void add_canvas();

  // (0,0) is top-left
  void set_viewport(const Vec2& extents);
  void set_viewport(const Vec2& top_left, const Vec2& extents);

  void set_fill(const D3DXCOLOR& col);
  void set_stroke(const D3DXCOLOR& col);

  void clear(const D3DXCOLOR& col);

  void set_circle_segments(int num_segments);
  void circle(const Vec2& o, float r);
  void circle(const Vec2& o, float r, int segments);

  void rect(const Vec2& top_left, const Vec2& size);

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
			: extents(Vec2(2,2))
			, top_left(-1,1)
		{
		}

		bool init()
		{
			RETURN_ON_FAIL_BOOL_E(_verts.create(32 * 1024));
			return true;
		}

		Vec2 extents;
		Vec2 top_left;
		DynamicVb<PosCol> _verts;
	};

	EffectWrapper *_effect;
	CComPtr<ID3D11InputLayout> _layout;

  std::deque<State> _states;
	std::deque<Canvas> _canvases;
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
  _states.push_back(State());
	_canvases.push_back(Canvas());
	RETURN_ON_FAIL_BOOL_E(_canvases.back().init());
	
	_effect = new EffectWrapper();
	RETURN_ON_FAIL_BOOL_E(_effect->load_shaders(shader, sizeof(shader), "vsMain", NULL, "psMain"));

	RETURN_ON_FAIL_BOOL_E(InputDesc().
		add("SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0).
		add("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0).
		create(_layout, _effect));

  return true;
}

bool Thud::close()
{
	SAFE_DELETE(_effect);

  delete this;
  _instance = nullptr;
  return true;
}

void Thud::set_viewport(const Vec2& extents)
{
	_canvases.back().top_left = Vec2(0,0);
  _canvases.back().extents = extents;
}

void Thud::set_viewport(const Vec2& top_left, const Vec2& extents)
{
  _canvases.back().top_left = top_left;
  _canvases.back().extents = extents;
}

void Thud::set_fill(const D3DXCOLOR& col)
{
  _states.back().fill = col;
}

void Thud::set_stroke(const D3DXCOLOR& col)
{
  _states.back().stroke = col;
}

void Thud::clear(const D3DXCOLOR& col)
{

}

void Thud::start_frame()
{

}

void Thud::render()
{

}

void Thud::circle(const Vec2& o, float r)
{

}

void Thud::rect(const Vec2& top_left, const Vec2& size)
{

}

void Thud::push_state()
{
  _states.push_back(State());
}

void Thud::pop_state()
{
  _states.pop_back();   
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

  thud.set_viewport(Vec2(-1, 1), Vec2(2, 2));

  MSG msg;
  ZeroMemory(&msg, sizeof(msg));
  while (WM_QUIT != msg.message) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      thud.clear(D3DXCOLOR(0, 1, 0, 1));
      thud.circle(Vec2(0,0), 0.5f);
      graphics.present();
    }
  }


  thud.close();
  graphics.close();

  return (int)msg.wParam;
}
