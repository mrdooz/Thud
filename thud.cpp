#include "stdafx.h"
#include <windows.h>
#include <deque>
#include <celsus/graphics.hpp>
#include <celsus/dynamic_vb.hpp>
#include <celsus/vertex_types.hpp>
#include <celsus/error2.hpp>
#include <celsus/Logger.hpp>
#include <celsus/effect_wrapper.hpp>
#include <celsus/math_utils.hpp>
#include <D3DX10math.h>

using namespace std;

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
"	o.pos = v.pos * scale;			  "\
"	o.col = v.col;											"\
"	return o;														"\
"}																		"\
"																			"\
"float4 psMain(in psInput v) : SV_Target	"\
"{"\
"	return v.col;"\
"}";

struct ScreenToClip
{
	D3DXVECTOR2 to_clip(float x, float y);
	D3DXVECTOR2 to_screen(float x, float y);

	D3DXVECTOR2 screen_extents;
	D3DXVECTOR2 clip_origin;
	D3DXVECTOR2 clip_extents;
};

D3DXVECTOR2 ScreenToClip::to_clip(float x, float y)
{
	// cx=(2*(sx-cex/2))/sex
	// cy=(2*(sey/2-sy))/sey

	return D3DXVECTOR2(
		-clip_extents.x / 2 + 2 * x / screen_extents.x,
		clip_extents.y / 2 - 2  * y / screen_extents.y);
}

D3DXVECTOR2 ScreenToClip::to_screen(float x, float y)
{
	// sx=(cx*sex+cex)/2
	// sy=-((cy-1)*sey)/2

	return D3DXVECTOR2(
		(x * screen_extents.x + clip_extents.x)/2,
		-((y - 1) * screen_extents.y)/ 2);

}

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

	void line(const D3DXVECTOR3& p0, const D3DXVECTOR3& p1, float w);

  void start_frame();
  void render();

  struct State
  {
    State()
      : circle_segments(40)
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
      : ptr(nullptr)
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

	ScreenToClip _screen_to_clip;
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
		add("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12).
		create(_layout, _effect));

  RETURN_ON_FAIL_BOOL_E(create_cbuffer(Graphics::instance().device(), 1 * sizeof(D3DXVECTOR4), &_cbuffer.p));

	set_extents(D3DXVECTOR2((float)Graphics::instance().width(), (float)Graphics::instance().height()));

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
	_screen_to_clip.screen_extents = extents;
	_screen_to_clip.clip_origin = D3DXVECTOR2(0,0);
	_screen_to_clip.clip_extents = D3DXVECTOR2(2, 2);
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
	scale->x = scale->y = scale->z = scale->w = 1;
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
  set_vb(context, canvas.verts.get(), canvas.verts.stride);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->Draw(num_verts, 0);
}

void Thud::circle(const D3DXVECTOR3& o, float r)
{
  const State& state = _state_stack.back();
  Canvas& canvas = _canvas_stack.back();
  PosCol*& ptr = canvas.ptr;
  const float inc = 2 * (float)kPi / (state.circle_segments);
  float ofs = 0;
  D3DXVECTOR3 cur = D3DXVECTOR3(o.x + r * cosf(ofs), o.y + r * sinf(ofs), o.z);
  for (int i = 0; i < state.circle_segments; ++i) {
    D3DXVECTOR3 next = D3DXVECTOR3(o.x + r * cosf(ofs + inc), o.y + r * sinf(ofs + inc), o.z);
    *ptr++ = PosCol(_screen_to_clip.to_clip(o.x, o.y), o.z, state.fill);
    *ptr++ = PosCol(_screen_to_clip.to_clip(cur.x, cur.y), cur.z, state.fill);
		*ptr++ = PosCol(_screen_to_clip.to_clip(next.x, next.y), next.z, state.fill);
    cur = next;
    ofs += inc;
  }
}

void Thud::rect(const D3DXVECTOR3& top_left, const D3DXVECTOR3& size)
{
	const State& state = _state_stack.back();
	Canvas& canvas = _canvas_stack.back();
	PosCol*& ptr = canvas.ptr;

	// v0, v1
	// v2, v3
	const D3DXVECTOR3 v0 = top_left;
	const D3DXVECTOR3 v1 = top_left + D3DXVECTOR3(size.x, 0, 0);
	const D3DXVECTOR3 v2 = top_left + D3DXVECTOR3(0, size.y, 0);
	const D3DXVECTOR3 v3 = top_left + D3DXVECTOR3(size.x, size.y, 0);

	// v0, v1, v2
	*ptr++ = PosCol(_screen_to_clip.to_clip(v0.x, v0.y), v0.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v1.x, v1.y), v1.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v2.x, v2.y), v2.z, state.fill);

	// v2, v1, v3
	*ptr++ = PosCol(_screen_to_clip.to_clip(v2.x, v2.y), v2.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v1.x, v1.y), v1.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v3.x, v3.y), v3.z, state.fill);
}

void Thud::line(const D3DXVECTOR3& p0, const D3DXVECTOR3& p1, float w)
{
	const D3DXVECTOR3 n0 = vec3_normalize(D3DXVECTOR3(p0.y - p1.y, p1.x - p0.x, 0));
	const D3DXVECTOR3 n1 = vec3_normalize(D3DXVECTOR3(p1.y - p0.y, p0.x - p1.x, 0));

	// v0, v1
	// v2, v3
	const D3DXVECTOR3 v0 = p0 + 0.5f * w * n0;
	const D3DXVECTOR3 v1 = p1 + 0.5f * w * n0;
	const D3DXVECTOR3 v2 = p0 + 0.5f * w * n1;
	const D3DXVECTOR3 v3 = p1 + 0.5f * w * n1;

	const State& state = _state_stack.back();
	Canvas& canvas = _canvas_stack.back();
	PosCol*& ptr = canvas.ptr;

	// v0, v1, v2
	*ptr++ = PosCol(_screen_to_clip.to_clip(v0.x, v0.y), v0.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v2.x, v2.y), v2.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v1.x, v1.y), v1.z, state.fill);

	// v2, v1, v3
	*ptr++ = PosCol(_screen_to_clip.to_clip(v2.x, v2.y), v2.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v3.x, v3.y), v3.z, state.fill);
	*ptr++ = PosCol(_screen_to_clip.to_clip(v1.x, v1.y), v1.z, state.fill);

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

D3DXVECTOR3 bezier(float t, const D3DXVECTOR3& p0, const D3DXVECTOR3& p1, const D3DXVECTOR3& p2, const D3DXVECTOR3& p3)
{
	const float tt = (1-t);
	const float tt2 = tt*tt;
	const float tt3 = tt2*tt;

	const float t2 = t*t;
	const float t3 = t2*t;

	return tt3 * p0 + 3 * tt2 * t * p1 + 3 * tt * t2 * p2 + t3 * p3;
}

void console_printf(const char *fmt, ...)
{
  va_list arg;
  va_start(arg, fmt);

  const int len = _vscprintf(fmt, arg) + 1;
  char* buf = (char*)_alloca(len);
  vsprintf_s(buf, len, fmt, arg);
  OutputDebugStringA(buf);
}


template<typename T>
struct Matrix2d
{
  Matrix2d()
    : _data(NULL)
    , _rows(0)
    , _cols(0)
  {
  }

  Matrix2d(int rows, int cols) 
    : _rows(rows)
    , _cols(cols) 
  {
    _data = new T[rows * cols];
  }

  void init(int rows, int cols)
  {
    reset();
    _rows = rows;
    _cols = cols;
    _data = new T[rows*cols];
  }

  ~Matrix2d()
  {
    reset();
  }

  Matrix2d(const Matrix2d& rhs)
    : _data(NULL)
    , _rows(0)
    , _cols(0)
  {
    if (&rhs == this)
      return;

    assign(rhs);
  }

  Matrix2d& operator=(const Matrix2d& rhs)
  {
    reset();
    assign(rhs);
  }

  const T& at(int row, int col) const
  {
    return _data[row*_cols+col];
  }

  T& at(int row, int col)
  {
    return _data[row*_cols+col];
  }

  const T& operator()(int row, int col) const
  {
    return _data[row*_cols+col];
  }

  T& operator()(int row, int col)
  {
    return _data[row*_cols+col];
  }

  void reset()
  {
    delete [] _data;
    _data = NULL;
    _rows = _cols = 0;
  }

  void assign(const Matrix2d& rhs)
  {
    _rows = rhs._rows;
    _cols = rhs._cols;
    _data = new T[_rows * _cols];
  }

  int rows() const { return _rows; }
  int cols() const { return _cols; }

  void augment(const Matrix2d& a, Matrix2d *out)
  {
    // out = [this|a];
    out->init(rows(), cols() + a.cols());

    const int nr = out->rows();
    const int nc = out->cols();

    for (int i = 0; i < out->rows(); ++i) {
      memcpy(&out->_data[i*nc], &_data[i*cols()], cols()*sizeof(T));
      memcpy(&out->_data[i*nc+cols()], &a._data[i*a.cols()], a.cols()*sizeof(T));
    }
  }

  void console_print()
  {
    for (int i = 0; i < rows(); ++i) {
      for (int j = 0; j < cols(); ++j) {
        console_printf("%8f ", at(i,j));
      }
      console_printf("\n");
    }
    console_printf("\n");
  }

  void print()
  {
    for (int i = 0; i < rows(); ++i) {
      for (int j = 0; j < cols(); ++j) {
        printf("%8f ", at(i,j));
      }
      printf("\n");
    }
    printf("\n");
  }

  T* _data;
  int _rows;
  int _cols;
};


template<typename T>
void augment(const Matrix2d<T>& a, const Matrix2d<T>& b, Matrix2d<T> *out)
{
  // out = [a|b];
  out->init(a.rows(), a.cols() + b.cols());

  const int nr = out->rows();
  const int nc = out->cols();

  for (int i = 0; i < out->rows(); ++i) {
    memcpy(&out->_data[i*nc], &a._data[i*a.cols()], a.cols()*sizeof(T));
    memcpy(&out->_data[i*nc+a.cols()], &b._data[i*b.cols()], b.cols()*sizeof(T));
  }
}


template<typename T>
void gaussian_solve(Matrix2d<T>& c, Matrix2d<T> *x)
{
  // solve m*x = a via gaussian elimination
  // c is the augmented matrix, [m|a]

  x->init(c.rows(), 1);

  // TODO: do pivoting

  T eps = 0.00001f;

  // row reduction
  for (int i = 0; i < c.rows(); ++i) {
    const T v = c.at(i,i);
    // skip if we already have a leading 1
    if ((T)abs(1-v) < eps)
      continue;

    const T d = 1 / v;
    // make the leading element in the current row 1
    for (int j = i; j < c.cols(); ++j)
      c.at(i,j) = c.at(i,j) * d;

    // reduce the remaining rows to set 0s in the i:th column
    for (int j = i+1; j < c.rows(); ++j) {
      const T v = c.at(j,i);
      // skip if the element is 0 already
      if ((T)abs(v) < eps)
        continue;
      const T d = c.at(j,i) / c.at(i,i);
      for (int k = i; k < c.cols(); ++k)
        c.at(j,k) = c.at(j,k) - d * c.at(i,k);
    }
  }

  // backward substitution
  for (int i = 0; i < c.rows() - 1; ++i) {
    const int cur_row = c.rows() - 2 - i;
    const int start_col = cur_row + 1;
    for (int j = 0; j <= i; ++j) {
      const T s = c.at(cur_row, start_col+j);
      const T v = c.at(start_col+j, c.cols()-1);
      c.at(cur_row, c.cols()-1) = c.at(cur_row, c.cols()-1) - s * v;
      c.at(cur_row, start_col+j) = 0;
    }
  }

  // copy the solution
  for (int i = 0; i < c.rows(); ++i)
    x->at(i,0) = c.at(i,c.cols()-1);
}

template<typename T>
void gaussian_solve(const Matrix2d<T>& m, const Matrix2d<T>& a, Matrix2d<T> *x)
{
  // solve m*x = a via gaussian elimination
  assert(m.rows() == a.rows());
  assert(m.rows() == m.cols());

  // TODO: do pivoting

  // c = [m|a]
  Matrix2d<T> c;
  augment(m, a, &c);
  gaussian_solve(c, &x);
}

// wrapper around a <data,size> tuple
template<typename T>
class AsArray
{
public:
  AsArray(T* data, int n) : _data(data), _n(n) {}
  int size() const { return _n; }
  T *data() { return _data; }
private:
  T *_data;
  int _n;
};

struct Bezier
{
  static Bezier from_points(AsArray<D3DXVECTOR3> points)
  {
    assert(points.size() >= 4);

    // Create a Bezier curve that passes through all the
    // given points.

    // Create a B-spline, and determine the control points
    // that pass throught the given points

    Matrix2d<float> m;
    const int size = points.size() - 2;
    m.init(size, size+1);

    D3DXVECTOR3 *pts = (D3DXVECTOR3 *)_alloca(sizeof(D3DXVECTOR3) * points.size());
    D3DXVECTOR3 *d = points.data();
    pts[0] = d[0];
    pts[points.size()-1] = d[points.size()-1];

    // solve for each coordinate
    for (int c = 0; c < 3; ++c) {

      // build the 1-4-1 matrix
      for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
          m.at(i,j) = 
            (j == i - 1 || j == i + 1) ? 1.0f :
            j == i ? 4.0f :
            0.0f;
        }
      }

      // add the control points to the last column
      for (int i = 0; i < size; ++i)
        m.at(i, size) = 
        i == 0 ? (6*d[1][c] - d[0][c]) :
        i == size - 1 ? (6*d[size][c] - d[size+1][c]) :
        6 * d[i+1][c];

      // solve..
      Matrix2d<float> x;
      gaussian_solve(m, &x);

      // TODO: fix
      for (int i = 0; i < size; ++i)
        pts[i+1][c] = x.at(i,0);

    }

    Bezier b;

    // there are points-1 bezier curves
    for (int i = 0; i < points.size()-1; ++i)
      b.curves.push_back(ControlPoints(
      d[i+0],
      2*pts[i+0]/3 + 1*pts[i+1]/3,
      1*pts[i+0]/3 + 2*pts[i+1]/3,
      d[i+1]));

    return b;
  }

  D3DXVECTOR3 interpolate(float t)
  {
    int ofs = max(0, min((int)curves.size()-1,(int)t));
    t = max(0, min(1,t - ofs));

    const ControlPoints& pts = curves[ofs];

    const float tt = (1-t);
    const float tt2 = tt*tt;
    const float tt3 = tt2*tt;

    const float t2 = t*t;
    const float t3 = t2*t;

    return tt3 * pts.p0 + 3 * tt2 * t * pts.p1 + 3 * tt * t2 * pts.p2 + t3 * pts.p3;

  }


  struct ControlPoints
  {
    ControlPoints(const D3DXVECTOR3& p0, const D3DXVECTOR3& p1, const D3DXVECTOR3& p2, const D3DXVECTOR3& p3) : p0(p0), p1(p1), p2(p2), p3(p3) {}
    D3DXVECTOR3 p0, p1, p2, p3;
  };

  std::vector<ControlPoints> curves;


};



int WINAPI WinMain( __in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPSTR lpCmdLine, __in int nShowCmd )
{

  static TCHAR window_class[] = _T("Thud Main Window");
  const int width = GetSystemMetrics(SM_CXSCREEN) / 2;
  const int height = GetSystemMetrics(SM_CYSCREEN) / 2;
	const float apsect = (float)width / height;

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

  MSG msg;
  ZeroMemory(&msg, sizeof(msg));

  D3DXVECTOR3 pts[] = 
  {
    D3DXVECTOR3(0, 0, 0),
    D3DXVECTOR3(160, 100, 0),
    D3DXVECTOR3(500, 200, 0),
    D3DXVECTOR3(200, 500, 0),
    D3DXVECTOR3((float)width, (float)height, 0),
  };

  const int num = ELEMS_IN_ARRAY(pts);

  Bezier bb = Bezier::from_points(AsArray<D3DXVECTOR3>(pts, num));

  while (WM_QUIT != msg.message) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      graphics.clear(D3DXCOLOR(0.5f, 0.5f, 0.5f, 1));
      thud.start_frame();
      thud.clear(D3DXCOLOR(0, 1, 0, 1));
      thud.set_fill(D3DXCOLOR(1,1,1,1));
			//thud.circle(D3DXVECTOR3(width/2.0f, height/2.0f,0.5f), width/4.0f);
			//thud.rect(D3DXVECTOR3(0,0,0), D3DXVECTOR3(width/2.0f, height/2.0f, 0));
			//thud.rect(D3DXVECTOR3(width/2,height/2,0), D3DXVECTOR3(width/2.0f, height/2.0f, 0));

			D3DXVECTOR3 prev = bb.interpolate(0); // bezier(0, D3DXVECTOR3(0, 200, 0), D3DXVECTOR3(40, 300, 0), D3DXVECTOR3(250, 0, 0), D3DXVECTOR3(500, 200, 0));
			for (int i = 0; i < num-1; ++i) {
				for (int j = 0; j <= 10; ++j) {
					D3DXVECTOR3 cur = bb.interpolate(i+j/10.0f); //bezier(i/10.0f + j/100.0f, D3DXVECTOR3(0, 200, 0), D3DXVECTOR3(40, 300, 0), D3DXVECTOR3(250, 0, 0), D3DXVECTOR3(500, 200, 0));
					thud.line(prev, cur, 1);
					prev = cur;
				}
			}
      thud.render();
      graphics.present();
    }
  }

  thud.close();
  graphics.close();

  return (int)msg.wParam;
}
