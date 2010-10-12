#include "stdafx.h"
#include <windows.h>
#include <deque>
#include <celsus/graphics.hpp>
#include <celsus/dynamic_vb.hpp>
#include <celsus/vertex_types.hpp>
#include <celsus/error2.hpp>
#include <celsus/Logger.hpp>
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

struct Thud
{
  static Thud& instance();

  bool init();
  bool close();

  void push_state();
  void pop_state();

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
      : extents(Vec2(2,2))
      , top_left(-1,1)
      , circle_segments(20)
      , fill(D3DXCOLOR(0,0,0,0))
      , stroke(D3DXCOLOR(1,1,1,1))
    {
    }
    Vec2 extents;
    Vec2 top_left;
    int circle_segments;
    D3DXCOLOR fill;
    D3DXCOLOR stroke;
  };

  DynamicVb<PosCol> _verts;

  std::deque<State> _states;
  static Thud *_instance;
};

Thud *Thud::_instance = nullptr;

Thud& Thud::instance()
{
  return !_instance ? *(_instance = new Thud) : *_instance;
}

bool Thud::init()
{
  // default state
  _states.push_back(State());

  RETURN_ON_FAIL_BOOL(_verts.create(32*1024), LOG_ERROR_LN);

  return true;
}

bool Thud::close()
{
  delete this;
  _instance = nullptr;
  return true;
}

void Thud::set_viewport(const Vec2& extents)
{
  _states.back().top_left = Vec2(0,0);
  _states.back().extents = extents;
}

void Thud::set_viewport(const Vec2& top_left, const Vec2& extents)
{
  _states.back().top_left = top_left;
  _states.back().extents = extents;
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
