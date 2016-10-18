typedef struct {
	int mInterface;
	void (*declare)(int,const char*,const char*);
} MetaGlue;

typedef struct {
	int uiInterface;
	void (*openVerticalBox)(int,const char*);
	void (*addNumEntry)(int,const char*, float* , float, float, float, float);
	void (*closeBox)(int);
	void (*declare) (int, float* , const char*, const char*);
	void (*addHorizontalSlider) (int, const char*, float*, float, float, float, float);
} UIGlue;

#define max( a, b ) ( ( a > b) ? a : b )
#define min( a, b ) ( ( a < b) ? a : b )
	