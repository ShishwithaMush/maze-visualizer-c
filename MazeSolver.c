/* maze_visualizer_fix.c
   Fixes: correct parent handling so reconstructed path is valid.
   Pure C, console "graphics" with ANSI background colors and double-space cells.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#endif

/* portable sleep ms */
static void sleep_ms(int ms) {
#if defined(_WIN32) || defined(_WIN64)
	Sleep(ms);
#else
	if (ms > 0) usleep(ms * 1000);
#endif
}

/* enable ANSI on Windows */
static void enable_ansi_on_windows(void) {
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE) return;
	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode)) return;
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, dwMode);
#endif
}

/* terminal helpers */
static void clear_screen(void) {
	printf("\x1b[2J");
}
static void move_cursor_home(void) {
	printf("\x1b[H");
}
static void hide_cursor(void) {
	printf("\x1b[?25l");
}
static void show_cursor(void) {
	printf("\x1b[?25h");
}

/* colors & blocks */
#define COL_RESET "\x1b[0m"
#define COL_WALL  "\x1b[48;2;20;28;36m"
#define COL_EMPTY "\x1b[48;2;240;245;250m"
#define COL_VISIT "\x1b[48;2;16;185;129m"
#define COL_FRONT "\x1b[48;2;96;165;250m"
#define COL_PATH  "\x1b[48;2;244;63;94m"
#define COL_SE    "\x1b[48;2;251;191;36m"
#define FULL_BLOCK "  "

typedef unsigned char cell_t;
typedef unsigned char mark_t;
#define M_NONE 0
#define M_VISIT 1
#define M_FRONT 2
#define M_PATH 4

typedef struct {
	int rows, cols;
	cell_t *cells;
	mark_t *marks;
} Grid;

static inline cell_t grid_get(const Grid *g, int r, int c) {
	return g->cells[r * g->cols + c];
}
static inline void grid_set(Grid *g, int r, int c, cell_t v) {
	g->cells[r * g->cols + c] = v;
}
static inline mark_t mark_get(const Grid *g, int r, int c) {
	return g->marks[r * g->cols + c];
}
static inline void mark_or(Grid *g, int r, int c, mark_t v) {
	g->marks[r * g->cols + c] |= v;
}
static inline void mark_andnot(Grid *g, int r, int c, mark_t v) {
	g->marks[r * g->cols + c] &= ~v;
}
static inline void mark_set(Grid *g, int r, int c, mark_t v) {
	g->marks[r * g->cols + c] = v;
}

static void grid_init(Grid *g, int rows, int cols) {
	g->rows = rows;
	g->cols = cols;
	g->cells = (cell_t*)malloc(rows * cols);
	g->marks = (mark_t*)malloc(rows * cols);
	if (!g->cells || !g->marks) {
		fprintf(stderr,"Out of memory\n");
		exit(1);
	}
	memset(g->cells, 1, rows * cols);
	memset(g->marks, M_NONE, rows * cols);
}
static void grid_free(Grid *g) {
	free(g->cells);
	free(g->marks);
	g->cells = NULL;
	g->marks = NULL;
}

static void shuffle_ints(int *arr, int n) {
	for (int i = n-1; i > 0; --i) {
		int j = rand() % (i+1);
		int t = arr[i];
		arr[i] = arr[j];
		arr[j] = t;
	}
}

/* generate perfect maze (iterative backtracker) */
typedef struct {
	int r,c;
} CellRC;

static void generate_maze(Grid *g) {
	int rows = g->rows, cols = g->cols;
	for (int r=0; r<rows; r++) for (int c=0; c<cols; c++) grid_set(g,r,c,1);
	for (int r=1; r<rows; r+=2) for (int c=1; c<cols; c+=2) grid_set(g,r,c,0);

	int maxcells = (rows/2)*(cols/2);
	CellRC *stack = malloc(maxcells * sizeof(CellRC));
	unsigned char *vis = calloc(rows*cols,1);
	int top = 0;
	stack[top++] = (CellRC) {
		1,1
	};
	vis[1*cols + 1] = 1;

	while (top > 0) {
		CellRC cur = stack[top-1];
		int r = cur.r, c = cur.c;
		int dirs[4][2] = {{-2,0},{2,0},{0,-2},{0,2}};
		int choices[4], ch=0;
		for (int i=0; i<4; i++) {
			int nr = r + dirs[i][0], nc = c + dirs[i][1];
			if (nr>0 && nr<rows-1 && nc>0 && nc<cols-1) {
				if (!vis[nr*cols + nc]) choices[ch++]=i;
			}
		}
		if (ch>0) {
			int pick = choices[rand()%ch];
			int nr = r + dirs[pick][0], nc = c + dirs[pick][1];
			int wr = r + dirs[pick][0]/2, wc = c + dirs[pick][1]/2;
			grid_set(g, wr, wc, 0);
			vis[nr*cols + nc]=1;
			stack[top++] = (CellRC) {
				nr,nc
			};
		} else {
			--top;
		}
	}
	free(stack);
	free(vis);
}

/* draw */
static void draw_grid(const Grid *g, int sr, int sc, int er, int ec) {
	move_cursor_home();
	for (int r=0; r<g->rows; r++) {
		for (int c=0; c<g->cols; c++) {
			if (r==sr && c==sc) {
				printf("%s%s%s", COL_SE, FULL_BLOCK, COL_RESET);
				continue;
			}
			if (r==er && c==ec) {
				printf("%s%s%s", COL_SE, FULL_BLOCK, COL_RESET);
				continue;
			}
			cell_t cell = grid_get(g,r,c);
			mark_t m = mark_get(g,r,c);
			if (cell==1) printf("%s%s%s", COL_WALL, FULL_BLOCK, COL_RESET);
			else if (m & M_PATH) printf("%s%s%s", COL_PATH, FULL_BLOCK, COL_RESET);
			else if (m & M_FRONT) printf("%s%s%s", COL_FRONT, FULL_BLOCK, COL_RESET);
			else if (m & M_VISIT) printf("%s%s%s", COL_VISIT, FULL_BLOCK, COL_RESET);
			else printf("%s%s%s", COL_EMPTY, FULL_BLOCK, COL_RESET);
		}
		printf("\n");
	}
	fflush(stdout);
}

/* small data structures */
typedef struct {
	CellRC *data;
	int top, cap;
} Stack;
static Stack *stack_create(int cap) {
	Stack*s=malloc(sizeof(Stack));
	s->data=malloc(sizeof(CellRC)*cap);
	s->top=0;
	s->cap=cap;
	return s;
}
static void stack_push(Stack*s, CellRC v) {
	if (s->top < s->cap) s->data[s->top++]=v;
}
static CellRC stack_pop(Stack*s) {
	return s->data[--s->top];
}
static int stack_empty(Stack*s) {
	return s->top==0;
}
static void stack_free(Stack*s) {
	free(s->data);
	free(s);
}

typedef struct {
	CellRC *data;
	int head, tail, cap;
} Queue;
static Queue* queue_create(int cap) {
	Queue*q=malloc(sizeof(Queue));
	q->data=malloc(sizeof(CellRC)*cap);
	q->head=q->tail=0;
	q->cap=cap;
	return q;
}
static void queue_push(Queue*q, CellRC v) {
	q->data[q->tail++]=v;
	if (q->tail>=q->cap) q->tail=0;
}
static CellRC queue_pop(Queue*q) {
	CellRC v=q->data[q->head++];
	if (q->head>=q->cap) q->head=0;
	return v;
}
static int queue_empty(const Queue*q) {
	return q->head==q->tail;
}
static void queue_free(Queue*q) {
	free(q->data);
	free(q);
}

/* helpers */
static int is_inside(const Grid*g,int r,int c) {
	return r>=0 && r<g->rows && c>=0 && c<g->cols;
}
static const int nbrs4[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};

/* reconstruct path using parent[] (only if parent set) */
static void reconstruct_and_mark(Grid *g, int *parent, int cols, int er, int ec, int delay_ms) {
	int idx = er * cols + ec;
	if (parent[idx] == -1) return; /* no path */
	int cur = idx;
	while (cur != -2 && cur != -1) {
		int rr = cur / cols, cc = cur % cols;
		mark_or(g, rr, cc, M_PATH);
		cur = parent[cur];
		draw_grid(g, /*sr*/1, /*sc*/1, er, ec); /* we pass sr/sc just for drawing */
		sleep_ms(delay_ms);
	}
}

/* BFS - shortest path */
static void solve_bfs(Grid *g, int sr, int sc, int er, int ec, int delay_ms) {
	int rows = g->rows, cols = g->cols;
	int *parent = malloc(sizeof(int)*rows*cols);
	for (int i=0; i<rows*cols; i++) parent[i] = -1;
	memset(g->marks, M_NONE, rows*cols);

	Queue *q = queue_create(rows*cols + 5);
	queue_push(q, (CellRC) {
		sr,sc
	});
	parent[sr*cols + sc] = -2; /* root */
	mark_or(g, sr, sc, M_FRONT);

	while (!queue_empty(q)) {
		CellRC cur = queue_pop(q);
		int r=cur.r, c=cur.c;
		mark_andnot(g, r, c, M_FRONT);
		if (!(g->marks[r*cols + c] & M_VISIT)) {
			mark_or(g, r, c, M_VISIT);
			draw_grid(g, sr, sc, er, ec);
			sleep_ms(delay_ms);
		}
		if (r==er && c==ec) break;
		for (int k=0; k<4; k++) {
			int nr=r + nbrs4[k][0], nc = c + nbrs4[k][1];
			if (is_inside(g,nr,nc) && grid_get(g,nr,nc)==0 && parent[nr*cols + nc] == -1) {
				parent[nr*cols + nc] = r*cols + c; /* set parent only once when discovered */
				queue_push(q, (CellRC) {
					nr,nc
				});
				mark_or(g, nr, nc, M_FRONT);
			}
		}
	}
	reconstruct_and_mark(g, parent, cols, er, ec, delay_ms);
	queue_free(q);
	free(parent);
}

/* DFS iterative - parent set only when discovered (prevents wrong overwrites) */
static void solve_dfs(Grid *g, int sr, int sc, int er, int ec, int delay_ms) {
	int rows = g->rows, cols = g->cols;
	int *parent = malloc(sizeof(int)*rows*cols);
	for (int i=0; i<rows*cols; i++) parent[i] = -1;
	memset(g->marks, M_NONE, rows*cols);

	Stack *st = stack_create(rows*cols + 5);
	stack_push(st, (CellRC) {
		sr,sc
	});
	parent[sr*cols + sc] = -2;
	mark_or(g, sr, sc, M_FRONT);

	while (!stack_empty(st)) {
		CellRC cur = stack_pop(st);
		int r = cur.r, c = cur.c;
		mark_andnot(g, r, c, M_FRONT);

		if (!(g->marks[r*cols + c] & M_VISIT)) {
			mark_or(g, r, c, M_VISIT);
			draw_grid(g, sr, sc, er, ec);
			sleep_ms(delay_ms);
		}
		if (r==er && c==ec) break;

		int order[4] = {0,1,2,3};
		shuffle_ints(order,4);
		for (int i=0; i<4; i++) {
			int k = order[i];
			int nr = r + nbrs4[k][0], nc = c + nbrs4[k][1];
			if (is_inside(g,nr,nc) && grid_get(g,nr,nc)==0 && g->marks[nr*cols + nc] == M_NONE) {
				/* If parent not set, set it now and push */
				if (parent[nr*cols + nc] == -1) parent[nr*cols + nc] = r*cols + c;
				stack_push(st, (CellRC) {
					nr,nc
				});
				mark_or(g, nr, nc, M_FRONT);
			}
		}
	}

	reconstruct_and_mark(g, parent, cols, er, ec, delay_ms);
	stack_free(st);
	free(parent);
}

/* helper input */
static int get_int_with_default(const char *prompt, int def) {
	char buf[128];
	printf("%s (default %d): ", prompt, def);
	if (!fgets(buf, sizeof(buf), stdin)) return def;
	int x;
	if (sscanf(buf, "%d", &x) == 1) return x;
	return def;
}

int main(void) {
	srand((unsigned)time(NULL));
	enable_ansi_on_windows();
	hide_cursor();
	atexit(show_cursor);

	printf("\nMAZE VISUALIZER- C\n");
	
	int cols = get_int_with_default("Enter odd number of columns", 31);
	int rows = get_int_with_default("Enter odd number of rows", 21);
	if (cols < 11) cols = 11;
	if (rows < 11) rows = 11;
	if (cols % 2 == 0) cols++;
	if (rows % 2 == 0) rows++;

	int algo_choice = get_int_with_default("Choose algorithm: 1=DFS (explore), 2=BFS (shortest)", 2);
	int delay = get_int_with_default("Animation delay in ms (0..200), smaller -> faster", 40);

	Grid g;
	grid_init(&g, rows, cols);
	int sr = 1, sc = 1, er = rows-2, ec = cols-2;

	while (1) {
		generate_maze(&g);
		clear_screen();
		move_cursor_home();
		draw_grid(&g, sr, sc, er, ec);
		printf("\nGenerated maze %dx%d. Press Enter to start solver", cols, rows);
		fflush(stdout);
		getchar();

		if (algo_choice == 1) solve_dfs(&g, sr, sc, er, ec, delay);
		else solve_bfs(&g, sr, sc, er, ec, delay);

		draw_grid(&g, sr, sc, er, ec);
		printf("\nSolver finished. Options:\n[r] Regenerate  [a] Toggle algorithm  [q] Quit\n");
		int c = getchar();
		if (c == '\n') c = getchar();
		if (c == 'q' || c == 'Q') break;
		if (c == 'a' || c == 'A') {
			algo_choice = (algo_choice==1) ? 2 : 1;
			printf("Toggled algorithm to %s\n", algo_choice==1?"DFS":"BFS");
			printf("Press Enter: ");
			getchar();
		}
	}

	grid_free(&g);
	clear_screen();
	show_cursor();
	printf("Thank you!\n");
	return 0;
}
