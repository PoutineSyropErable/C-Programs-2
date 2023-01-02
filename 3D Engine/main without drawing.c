// https://www.youtube.com/watch?v=TMFMpCabbDU&list=PLPGNrHeloF_dOY7ZlCq6D4UwKA-adaQHX&index=3&ab_channel=NickWalton
// I added it here.
#include <Windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <wingdi.h>
#include <math.h>
#include <time.h>

int window_top_left_x = 300; // These are only the initiation values...
int window_top_left_y = 100;
int window_height = 800;
int window_width = 1200;
int nbr_frame_rendered = 0;
static bool quit = false;

struct // This is the structure containing the frame data.
{
    int width;
    int height;
    uint32_t *pixels;
} frame = {0};

// the keyboard and mouse

bool keyboard[256] = {0};
struct
{
    int x, y;
    uint8_t buttons;
} mouse;

enum
{
    MOUSE_LEFT = 0b1,
    MOUSE_MIDDLE = 0b10,
    MOUSE_RIGHT = 0B100,
    MOUSE_X1 = 0B100,
    MOUSE_X2 = 0B10000
};

//----------------------------------------------------This is the calculation stuff
float fov = 90.0;
const float PI = 3.141592654;

// Let's create a structure.
typedef struct
{

    int len; // default length
    // int struct_size;

    //  |   x  4 - >
    //  v
    //  n
    // n rows, 4 collums. So, it's the oppositve of matrix multiplication. I have to do (A)B : Right multiplication.

    float mat[][4]; // (It's n)

    // The size of the structure is: 4(len integer) + 4(wxyz)*4(float)*n(the number of points)
} VertexMatrix;

typedef struct
{
    // A FaceMatrix has n face of 3 points
    int len;
    int mat[][3]; // The matrix with point indices
} FaceMatrix;

typedef struct
{
    int len;
    float list[];
} ListFloat;

typedef struct
{
    int len;
    int list[];
} ListInt;

typedef struct
{
    float *x;
    float *y;
    float *z;
} Point3D; // using pointers (euh... yeah let's do it.)

typedef struct
{
    float x;
    float y;
    float z; // the depth
} Point2D;

typedef struct
{
    // world space coordinates.
    Point3D *p1;
    Point3D *p2;
    Point3D *p3;
    Point3D *center;

    // screen space points
    Point2D *ss_p1; // screen space point 1;
    Point2D *ss_p2;
    Point2D *ss_p3;

    Point3D *normal_vector;
    bool shown;
} PolygonFace;

typedef struct
{
    // A FaceMatrix has n face of 3 points
    int len;
    float mat[][3]; // The matrix with point indices
} VertexMatrix2D;

typedef struct
{
    // The world space vertex matrix
    VertexMatrix *pVertex_matrix; // A pointer to an (VertexMatrix object). This variable is named VertexMatrix.
    // The Face List data
    FaceMatrix *pFace_matrix;

    // The order of the face rendering
    ListInt *pFace_render_order;

    // The view space vertex matrix
    VertexMatrix *pView_space_vm;

    // The screen space vertex matrix (2d)
    VertexMatrix2D *pScreen_space_vm;

    // All the normals (No repeats)
    VertexMatrix *pNormal_matrix;

    ListInt *pFace_normal_index;
    // The list of Face
    PolygonFace *pFace_list[];
} Object3D;

VertexMatrix2D *createVertexMatrix2D(int len)
{
    VertexMatrix2D *vm_2d = malloc(sizeof(float) * 3 * len);
    vm_2d->len = len;
    for (int i = 0; i < len; i++)
    {
        vm_2d->mat[i][0] = 0.f;
        vm_2d->mat[i][1] = 0.f;
        vm_2d->mat[i][2] = 0.f;
    }
    return vm_2d;
}

VertexMatrix *createVertexMatrix(int len)
{
    VertexMatrix *vm = malloc(sizeof(int) + sizeof(float) * len * 4);
    vm->len = len;

    for (int i = 0; i < len; i++)
    {
        vm->mat[i][0] = 0.f;
        vm->mat[i][1] = 0.f;
        vm->mat[i][2] = 0.f;
        vm->mat[i][3] = 1.f;
    }

    return vm;
}

FaceMatrix *createFaceMatrix(int len)
{
    FaceMatrix *fm = malloc(sizeof(int) + sizeof(float) * 3 * len);
    fm->len = len;

    for (int i = 0; i < len; i++)
    {
        fm->mat[i][0] = 0;
        fm->mat[i][1] = 0;
        fm->mat[i][2] = 0;
        // printf("[ %f %f %f ]", fm->mat[i][0], fm->mat[i][1], fm->mat[i][2]);
    }

    return fm;
}

ListFloat *createListFloat(int len)
{
    ListFloat *list = malloc(sizeof(float) * len + sizeof(int));
    list->len = len;

    for (int i = 0; i < len; i++)
        list->list[i] = 0;

    return list;
}

ListInt *createListInt(int len)
{
    ListInt *list = malloc(sizeof(int) * len + sizeof(int));
    list->len = len;

    for (int i = 0; i < len; i++)
        list->list[i] = 0;

    return list;
}

Point3D *createPoint3D(float *x, float *y, float *z)
{
    Point3D *point_3d = malloc(sizeof(float *) * 3);
    point_3d->x = x;
    point_3d->y = y;
    point_3d->z = z;
    return point_3d;
}

Point2D *createPoint2D(float x, float y, float z)
{
    Point2D *point_2d = malloc(sizeof(float) * 3);
    point_2d->x = x;
    point_2d->y = y;
    point_2d->z = z;
    return point_2d;
}

PolygonFace *createPolygonFace(Point3D *pPoint1, Point3D *pPoint2, Point3D *pPoint3, Point3D *normal_vector)
{
    PolygonFace *face = malloc(sizeof(PolygonFace)); // This runs at compile time. So, It can see the future.
    // Because The compiler will first detect the size of it and then create it.
    face->p1 = pPoint1;
    face->p2 = pPoint2;
    face->p3 = pPoint3;

    float *center_x, *center_y, *center_z;
    center_x = malloc(sizeof(float));
    center_y = malloc(sizeof(float));
    center_z = malloc(sizeof(float));

    *center_x = (*(pPoint1->x) + *(pPoint2->x) + *(pPoint3->x)) / (3.f);
    *center_y = (*(pPoint1->y) + *(pPoint2->y) + *(pPoint3->y)) / (3.f);
    *center_z = (*(pPoint1->z) + *(pPoint2->z) + *(pPoint3->z)) / (3.f);

    Point3D *center_point = createPoint3D(center_x, center_y, center_z);

    face->center = center_point;

    face->ss_p1 = createPoint2D(0.f, 0.f, 0.f);
    face->ss_p2 = createPoint2D(0.f, 0.f, 0.f);
    face->ss_p3 = createPoint2D(0.f, 0.f, 0.f);

    face->shown = true;
    face->normal_vector = normal_vector;

    return face;
}

Object3D *createObject3D(VertexMatrix *pVertex_Matrix, FaceMatrix *pFace_Matrix,
                         VertexMatrix *pNormal_Matrix, ListInt *pFace_normal_index)
{
    // pointer to Vertex_matrix
    //  pVm + pFm + pFro + pVsm + pSp + pNm + pFni= 5
    int memory_size = sizeof(int *) * (7 + pFace_Matrix->len); //( 5 + len(list) pointers)
    Object3D *object_3d = malloc(memory_size);

    object_3d->pVertex_matrix = pVertex_Matrix;
    object_3d->pFace_matrix = pFace_Matrix;
    object_3d->pNormal_matrix = pNormal_Matrix;
    object_3d->pFace_normal_index = pFace_normal_index;

    object_3d->pFace_render_order = createListInt(pFace_Matrix->len);
    for (int i = 0; i < pFace_Matrix->len; i++)
    {
        object_3d->pFace_render_order->list[i] = i;
    }

    object_3d->pView_space_vm = createVertexMatrix(pVertex_Matrix->len);

    object_3d->pScreen_space_vm = createVertexMatrix2D(pVertex_Matrix->len);

    // printf("\n");
    // print_FaceMatrix(pFace_Matrix);
    // printf("\n");

    for (int i = 0; i < pFace_Matrix->len; i++)
    {
        int index_face_i_p1 = pFace_Matrix->mat[i][0];
        int index_face_i_p2 = pFace_Matrix->mat[i][1];
        int index_face_i_p3 = pFace_Matrix->mat[i][2];

        float *p1_x = &(pVertex_Matrix->mat[index_face_i_p1][0]);
        float *p1_y = &(pVertex_Matrix->mat[index_face_i_p1][1]);
        float *p1_z = &(pVertex_Matrix->mat[index_face_i_p1][2]);

        float *p2_x = &(pVertex_Matrix->mat[index_face_i_p2][0]);
        float *p2_y = &(pVertex_Matrix->mat[index_face_i_p2][1]);
        float *p2_z = &(pVertex_Matrix->mat[index_face_i_p2][2]);

        float *p3_x = &(pVertex_Matrix->mat[index_face_i_p3][0]);
        float *p3_y = &(pVertex_Matrix->mat[index_face_i_p3][1]);
        float *p3_z = &(pVertex_Matrix->mat[index_face_i_p3][2]);

        Point3D *point1 = createPoint3D(p1_x, p1_y, p1_z);
        Point3D *point2 = createPoint3D(p2_x, p2_y, p2_z);
        Point3D *point3 = createPoint3D(p3_x, p3_y, p3_z);

        float *n_x, *n_y, *n_z;
        int normal_index_i = pFace_normal_index->list[i];
        n_x = &(object_3d->pNormal_matrix->mat[normal_index_i][0]);
        n_y = &(object_3d->pNormal_matrix->mat[normal_index_i][1]);
        n_z = &(object_3d->pNormal_matrix->mat[normal_index_i][2]);
        Point3D *normal_vector = createPoint3D(n_x, n_y, n_z);

        PolygonFace *face_i = createPolygonFace(point1, point2, point3, normal_vector);
        object_3d->pFace_list[i] = face_i;
    }

    return object_3d;
}

void set_VertexMatrix(VertexMatrix *vm, float matrix_to_copy[][4])
{
    for (int i = 0; i < vm->len; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            vm->mat[i][j] = matrix_to_copy[i][j]; // matrix_to_copy[i][j];
        }
    }
    // delete(matrix_to_copy);
}

void update_VertexMatrix(VertexMatrix *vm, VertexMatrix *vm_2)
{
    for (int i = 0; i < vm->len; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            vm->mat[i][j] = vm_2->mat[i][j]; // matrix_to_copy[i][j];
        }
    }
    // delete(matrix_to_copy);
}

void set_FaceMatrix(FaceMatrix *fm, int matrix_to_copy[][3])
{
    for (int i = 0; i < fm->len; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            fm->mat[i][j] = matrix_to_copy[i][j]; // matrix_to_copy[i][j];
        }
    }
}

//--------------------The prints function-------------------------------
void print_VertexMatrix(VertexMatrix *vm)
{
    printf("{{ ");
    for (int i = 0; i < vm->len; i++)
    {
        if (i > 0)
        {
            printf(" { ");
        }
        for (int j = 0; j < 4; j++)

        {
            printf("%f", vm->mat[i][j]);
            if (j < 3)
            {
                printf(",");
            }
            printf(" ");
        }
        if (i < vm->len - 1)
        {
            printf("}, \n");
        }
    }
    printf("}}");
}

void print_FaceMatrix(FaceMatrix *fm)
{
    printf("{{ ");
    for (int i = 0; i < fm->len; i++)
    {
        if (i > 0)
        {
            printf(" { ");
        }
        for (int j = 0; j < 3; j++)

        {
            printf("%d", fm->mat[i][j]);
            if (j < 2)
            {
                printf(",");
            }
            printf(" ");
        }
        if (i < fm->len - 1)
        {
            printf("}, \n");
        }
    }
    printf("}}");
}

void print_VertexMatrix2D(VertexMatrix2D *vm_2d)
{
    printf("{{ ");
    for (int i = 0; i < vm_2d->len; i++)
    {
        if (i > 0)
        {
            printf(" { ");
        }
        for (int j = 0; j < 3; j++)

        {
            printf("%f", vm_2d->mat[i][j]);
            if (j < 2)
            {
                printf(",");
            }
            printf(" ");
        }
        if (i < vm_2d->len - 1)
        {
            printf("}, \n");
        }
    }
    printf("}}");
}

void print_Point3D(Point3D *point)
{
    printf("p[x y z] = [ %p %p %p ]\n", (point->x), (point->y), (point->z));
    printf("[x y z] = [ %f %f %f ]\n", *(point->x), *(point->y), *(point->z));
}

void print_Point2D(Point2D *point)
{
    printf("[x y z] = [ %f %f %f ]\n", (point->x), (point->y), (point->z));
}

void print_ListInt(ListInt *list)
{
    printf("{ ");
    for (int i = 0; i < list->len; i++)
    {
        printf("%d ", list->list[i]);
    }
    printf("}\n");
}

VertexMatrix *Translation_Matrix(float shift_x, float shift_y, float shift_z)
{
    VertexMatrix *trans_mat = createVertexMatrix(4);
    trans_mat->mat[0][3] = trans_mat->mat[1][3] = trans_mat->mat[2][3] = 0.f;
    trans_mat->mat[0][0] = trans_mat->mat[1][1] = trans_mat->mat[2][2] = 1.f;
    trans_mat->mat[3][0] = -shift_x;
    trans_mat->mat[3][1] = -shift_y;
    trans_mat->mat[3][2] = -shift_z;
    return trans_mat;
}

VertexMatrix *Rx(float rot_x)
{

    VertexMatrix *rot_matrix_x = createVertexMatrix(4);
    rot_matrix_x->mat[0][0] = rot_matrix_x->mat[3][3] = 1.f;
    rot_matrix_x->mat[1][1] = rot_matrix_x->mat[2][2] = cosf(rot_x);

    rot_matrix_x->mat[0][3] = rot_matrix_x->mat[1][3] = rot_matrix_x->mat[2][3] = 0.f;
    float q = sinf(rot_x);
    rot_matrix_x->mat[2][1] = q;
    rot_matrix_x->mat[1][2] = -q;

    return rot_matrix_x; // Important to know I'm working with the transpose of
    // what I have in python and doing right mult rather then left mult.
}

VertexMatrix *Ry(float rot_y)
{

    VertexMatrix *rot_matrix_y = createVertexMatrix(4);
    rot_matrix_y->mat[1][1] = rot_matrix_y->mat[3][3] = 1.f;
    rot_matrix_y->mat[0][0] = rot_matrix_y->mat[2][2] = cosf(rot_y);

    rot_matrix_y->mat[0][3] = rot_matrix_y->mat[1][3] = rot_matrix_y->mat[2][3] = 0.f;
    float q = sinf(rot_y);
    rot_matrix_y->mat[2][0] = q;
    rot_matrix_y->mat[0][2] = -q;

    return rot_matrix_y;
}

VertexMatrix *Rz(float rot_z)
{

    VertexMatrix *rot_matrix_z = createVertexMatrix(4);
    rot_matrix_z->mat[2][2] = rot_matrix_z->mat[3][3] = 1.f;
    rot_matrix_z->mat[0][0] = rot_matrix_z->mat[1][1] = cosf(rot_z);

    rot_matrix_z->mat[0][3] = rot_matrix_z->mat[1][3] = rot_matrix_z->mat[2][3] = 0.f;
    float q = sinf(rot_z);
    rot_matrix_z->mat[1][0] = q;
    rot_matrix_z->mat[0][1] = -q;

    return rot_matrix_z;
}

VertexMatrix *mult(VertexMatrix *a, VertexMatrix *b)
{
    if (b->len != 4)
    {
        printf("\n\n\n ------------------------------------------\n");
        printf("The length wasn't 4 so I can't do matrix multiplication\n");
        printf(" Exiting the program");
        exit(0);
    }
    VertexMatrix *c = createVertexMatrix(a->len);

    for (int i = 0; i < a->len; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            c->mat[i][j] = 0;
            for (int k = 0; k < 4; k++)
            {
                c->mat[i][j] += a->mat[i][k] * b->mat[k][j];
            }
        }
    }

    return c;
}

float dot_product(Point3D *p1, Point3D *p2)
{
    float res = (*(p1->x)) * (*(p2->x)) + (*(p1->y)) * (*(p2->y)) + (*(p1->z)) * (*(p2->z));
    return res;
}

Point3D *Point3D_sub(Point3D *p1, Point3D *p2)
{
    float dx = *(p1->x) - *(p2->x);
    float dy = *(p1->y) - *(p2->y);
    float dz = *(p1->z) - *(p2->z);
    Point3D *point_res = createPoint3D(&dx, &dy, &dz);
    return point_res;
}

void get_face_shown(Object3D *object_3d, Point3D *player_pos)
{
    for (int i = 0; i < object_3d->pFace_matrix->len; i++)
    {
        Point3D *face_center_pos = object_3d->pFace_list[i]->center;
        Point3D *d_vector = Point3D_sub(player_pos, face_center_pos);

        float face_center_dot_product = dot_product(d_vector, object_3d->pFace_list[i]->normal_vector);
        float norm = (*player_pos->x) * (*player_pos->x) + (*player_pos->y) * (*player_pos->y) + (*player_pos->z) * (*player_pos->z);
        norm = sqrtf(norm);
        face_center_dot_product /= norm; // get fast inverse square root;
        if (face_center_dot_product < 0.f)
        {
            object_3d->pFace_list[i]->shown = false;
        }
        else
        {
            object_3d->pFace_list[i]->shown = true;
        }
    }
}

void set_view_space_matrix(Object3D *, VertexMatrix *);

void set_screen_space_matrix(Object3D *);

void project_and_perspective(Object3D *object_3d, float player_rot_x, float player_rot_y,
                             float player_rot_z, float player_pos_x, float player_pos_y, float player_pos_z)
{
    VertexMatrix *player_trans_matrix = Translation_Matrix(player_pos_x, player_pos_y, player_pos_z);
    // printf("\nThis is the translation matrix:\n");
    // print_VertexMatrix(player_trans_matrix);

    VertexMatrix *r_x = Rx(player_rot_x);
    VertexMatrix *r_y = Ry(player_rot_y);
    VertexMatrix *r_z = Rz(player_rot_z);

    VertexMatrix *player_rot_matrix = mult(r_z, r_y);
    player_rot_matrix = mult(player_rot_matrix, r_x);

    VertexMatrix *player_project_matrix = mult(player_trans_matrix, player_rot_matrix);

    set_view_space_matrix(object_3d, player_project_matrix);
    set_screen_space_matrix(object_3d);

    free(r_x);
    free(r_y);
    free(r_z);
    free(player_trans_matrix);
    free(player_rot_matrix);
    free(player_project_matrix);
}

void set_face_Point2D(Object3D *object_3d)
{
    for (int i = 0; i < object_3d->pFace_matrix->len; i++)
    {
        int index_face_i_p1 = object_3d->pFace_matrix->mat[i][0];
        int index_face_i_p2 = object_3d->pFace_matrix->mat[i][1];
        int index_face_i_p3 = object_3d->pFace_matrix->mat[i][2];

        float p1_x = object_3d->pScreen_space_vm->mat[index_face_i_p1][0];
        float p1_y = object_3d->pScreen_space_vm->mat[index_face_i_p1][1];
        float p1_z = object_3d->pScreen_space_vm->mat[index_face_i_p1][2];

        float p2_x = object_3d->pScreen_space_vm->mat[index_face_i_p2][0];
        float p2_y = object_3d->pScreen_space_vm->mat[index_face_i_p2][1];
        float p2_z = object_3d->pScreen_space_vm->mat[index_face_i_p2][2];

        float p3_x = object_3d->pScreen_space_vm->mat[index_face_i_p3][0];
        float p3_y = object_3d->pScreen_space_vm->mat[index_face_i_p3][1];
        float p3_z = object_3d->pScreen_space_vm->mat[index_face_i_p3][2];

        object_3d->pFace_list[i]->ss_p1->x = p1_x;
        object_3d->pFace_list[i]->ss_p1->y = p1_y;
        object_3d->pFace_list[i]->ss_p1->z = p1_z;

        object_3d->pFace_list[i]->ss_p2->x = p2_x;
        object_3d->pFace_list[i]->ss_p2->y = p2_y;
        object_3d->pFace_list[i]->ss_p2->z = p2_z;

        object_3d->pFace_list[i]->ss_p3->x = p3_x;
        object_3d->pFace_list[i]->ss_p3->y = p3_y;
        object_3d->pFace_list[i]->ss_p3->z = p3_z;

        if (p1_x >= frame.width / 2 || p1_x <= -frame.width / 2 || p1_y >= frame.height / 2 || p1_y <= -frame.height / 2)
        {
            object_3d->pFace_list[i]->shown = false;
        }

        if (p2_x >= frame.width / 2 || p2_x <= -frame.width / 2 || p2_y >= frame.height / 2 || p2_y <= -frame.height / 2)
        {
            object_3d->pFace_list[i]->shown = false;
        }

        if (p3_x >= frame.width / 2 || p3_x <= -frame.width / 2 || p3_y >= frame.height / 2 || p3_y <= -frame.height / 2)
        {
            object_3d->pFace_list[i]->shown = false;
        }
    }
}

int sign_of(float x)
{
    if (x < 0)
    {
        return -1;
    }
    else if (x > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

typedef struct
{
    int len;
    Object3D *pObject_3d_list[];
} Object3DList;

Object3DList *createObject3DList(int len)
{

    Object3DList *object_3d_list = malloc(sizeof(int) + sizeof(Object3D *) * len);
    object_3d_list->len = len;

    return object_3d_list;
}

void draw_pixel(int x, int y, COLORREF color)
{

    // frame.pixels[(int)(y + frame.height / 2) * frame.width + (x + frame.width / 2)] = color;
}

void draw_rectangle(int x_left, int x_right, int y_bottom, int y_top, COLORREF color)
{
    for (int x = x_left; x <= x_right; x++)
    {
        for (int y = y_bottom; y <= y_top; y++)
        {
            draw_pixel(x, y, color);
        }
    }
}

void draw_line(Point2D *p1, Point2D *p2, COLORREF color) // could redo it but in a smarter way
{
    int x1, x2, y1, y2;

    if (p1->x < p2->x)
    {
        x1 = p1->x, y1 = p1->y;
        x2 = p2->x, y2 = p2->y;
    }
    else
    {
        x1 = p2->x, y1 = p2->y;
        x2 = p1->x, y2 = p1->y;
    }

    int dx = x2 - x1;
    int dy = y2 - y1;
    float dy_over_dx = -1000;
    float dx_over_dy = -1000;
    if (dx != 0)
    {
        dy_over_dx = (float)dy / (float)dx;
    }
    if (dy != 0)
    {
        dx_over_dy = (float)dx / (float)dy;
    }

    if (dy_over_dx >= -1.f && dy_over_dx <= 1.f)
    {

        // draw a diagonal line
        for (int dx_temp = 0; dx_temp <= x2 - x1; dx_temp++)
        {
            draw_pixel((int)(x1 + dx_temp), (int)(y1 + dx_temp * dy_over_dx), color);
        }
    }
    else if (dx_over_dy >= -1.f && dx_over_dy <= 1.f)
    {
        if (y1 > y2)
        {
            int y_temp_switch = y1;
            int x_temp_switch = x1;
            y1 = y2;
            y2 = y_temp_switch;
            x1 = x2;
            x2 = x_temp_switch;
        }

        for (int dy_temp = 0; dy_temp <= y2 - y1; dy_temp++)
        {
            draw_pixel((int)(x1 + dy_temp * dx_over_dy), (int)(y1 + dy_temp), color);
        }
    }
    return;
}

void draw_triangle_mesh(Point2D *p1, Point2D *p2, Point2D *p3, COLORREF color)
{
    draw_line(p1, p2, color);
    draw_line(p2, p3, color);
    draw_line(p3, p1, color);
}

void render_PolygonFace(PolygonFace *face, COLORREF color)
{
    if (face->shown == true)
    {

        draw_triangle_mesh(face->ss_p1, face->ss_p2, face->ss_p3, color);
    }

    return;
}

void render_Object3D(Object3D *object_3d, COLORREF color)
{
    for (int i = 0; i < object_3d->pFace_render_order->len; i++)
    {
        int face_index_ordered = object_3d->pFace_render_order->list[i];

        render_PolygonFace(object_3d->pFace_list[face_index_ordered], color);
    }
}

void set_view_space_matrix(Object3D *object_3d, VertexMatrix *player_rot_matrix)
{
    VertexMatrix *object_3d_vs_temp = mult(object_3d->pVertex_matrix, player_rot_matrix);
    set_VertexMatrix(object_3d->pView_space_vm, object_3d_vs_temp->mat);
}

void set_screen_space_matrix(Object3D *object_3d)
{ // This assume that the view space is already calculated
    float fov_c = (1.f / (tanf(fov / 2.f)));
    for (int i = 0; i < object_3d->pFace_matrix->len; i++)
    {
        float temp_x = object_3d->pView_space_vm->mat[i][0];
        float temp_y = object_3d->pView_space_vm->mat[i][1];
        float temp_z = object_3d->pView_space_vm->mat[i][2];
        object_3d->pScreen_space_vm->mat[i][0] = 0.5f * frame.width * fov_c * (-temp_x / temp_z); //- important
        object_3d->pScreen_space_vm->mat[i][1] = 0.5f * frame.width * fov_c * (temp_y / temp_z);
        object_3d->pScreen_space_vm->mat[i][2] = temp_z;
        // I need a minus because of the way the coordinates are setup in 3D.
        // It's because in 3D , x is going left, z is forward, y is up. This way, it preserves
        // cross products.
    }
}

Object3DList *start_of_program()
{

    printf("\n\n ---------------- START OF PROGRAM ---------------- \n\n");
    float c_vm[8][4] =
        {{1.f, 1.f, -1.f, 1.f},
         {1.f, -1.f, -1.f, 1.f},
         {1.f, 1.f, 1.f, 1.f},
         {1.f, -1.f, 1.f, 1.f},
         {-1.f, 1.f, -1.f, 1.f},
         {-1.f, -1.f, -1.f, 1.f},
         {-1.f, 1.f, 1.f, 1.f},
         {-1.f, -1.f, 1.f, 1.f}};

    int cube_face_matrix[12][3] = {{0, 4, 6},
                                   {0, 6, 2},
                                   {3, 2, 6},
                                   {3, 6, 7},
                                   {7, 6, 4},
                                   {7, 4, 5},
                                   {5, 1, 3},
                                   {5, 3, 7},
                                   {1, 0, 2},
                                   {1, 2, 3},
                                   {5, 4, 0},
                                   {5, 0, 1}};

    int cube_face_normal_index[12] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    float cube_normal_matrix[6][4] = {{-0.f, 1.f, -0.f, 1.f},

                                      {0.f, 0.f, 1.f, 1.f},

                                      {-1.f, 0.f, 0.f, 1.f},

                                      {0.f, -1.f, 0.f, 1.f},

                                      {1.f, 0.f, 0.f, 1.f},

                                      {0.f, 0.f, -1.f, 1.f}};
    // New stuff
    VertexMatrix *vm_1 = createVertexMatrix(8);
    set_VertexMatrix(vm_1, c_vm);

    FaceMatrix *fm_1 = createFaceMatrix(12);
    set_FaceMatrix(fm_1, cube_face_matrix);

    VertexMatrix *normal_matrix = createVertexMatrix(6); // facelist->len /2 because triangles face.
    set_VertexMatrix(normal_matrix, cube_normal_matrix);

    ListInt *cfni = createListInt(12);
    for (int i = 0; i < cfni->len; i++)
        cfni->list[i] = cube_face_normal_index[i];

    Object3D *cube = createObject3D(vm_1, fm_1, normal_matrix, cfni);
    Object3DList *object_3d_list = createObject3DList(1);
    object_3d_list->pObject_3d_list[0] = cube;

    // COLORREF color2 = RGB(0,255,0);
    // draw_rectangle(-100,-100,-100,100,color2);

    return object_3d_list;
}

float cube_rot_x = 0;
float cube_rot_y = 0;
float cube_rot_z = 0;

void reset_screen(COLORREF color)
{
    for (int i = 0; i < frame.width * frame.height; i++)
    {
        frame.pixels[i] = color;
    }
}

void draw_on_screen(Object3DList *object_3d_list, Point3D *player_pos_global, Point3D *player_rot_global)
{
    COLORREF screen_color = RGB(0, 0, 0);
    reset_screen(screen_color);

    Object3D *cube = object_3d_list->pObject_3d_list[0];

    COLORREF color = RGB(255, 255, 255);

    draw_rectangle(-frame.width / 2 + 5, -frame.height / 2 + 5, -20, 20, color);

    float player_x = *(player_pos_global->x);
    float player_y = *(player_pos_global->y);
    float player_z = *(player_pos_global->z);
    float player_rx = *(player_rot_global->x);
    float player_ry = *(player_rot_global->y);
    float player_rz = *(player_rot_global->z);

    // printf("This is the player pos: x = %f | y = %f \n", player_x, player_y);
    // printf("This is the player rot: x = %f | y = %f \n", player_rx, player_ry);

    cube_rot_x = 0.001;
    cube_rot_y = 0.0023;
    cube_rot_z = 0.0017;

    VertexMatrix *cube_r_x = Rx(cube_rot_x);
    VertexMatrix *cube_r_y = Ry(cube_rot_y);
    VertexMatrix *cube_r_z = Rz(cube_rot_z);
    VertexMatrix *cube_rot = mult(cube_r_z, cube_r_y);
    cube_rot = mult(cube_rot, cube_r_x);

    VertexMatrix *temp_vertex = mult(cube->pVertex_matrix, cube_rot);
    VertexMatrix *temp_norm = mult(cube->pNormal_matrix, cube_rot);
    update_VertexMatrix(cube->pNormal_matrix, temp_norm);
    update_VertexMatrix(cube->pVertex_matrix, temp_vertex);
    get_face_shown(cube, player_pos_global);
    // when I calculate this, the memory adress changes.
    // cube->pFace_list[0]->normal_vector

    project_and_perspective(cube, -player_rx, -player_ry, -player_rz, player_x, player_y, player_z);
    set_face_Point2D(cube);

    render_Object3D(cube, color);
    printf("\nThis is the point position:\n");
    print_Point2D(cube->pFace_list[0]->ss_p1);

    free(temp_vertex);
    free(temp_norm);
    free(cube_r_x);
    free(cube_r_y);
    free(cube_r_z);
}

//---------------------------This is the windows stuff

LRESULT CALLBACK WindowProcessMessage(HWND, UINT, WPARAM, LPARAM);

// static BITMAPINFO frame_bitmap_info;
// static HBITMAP frame_bitmap = 0;
// static HDC frame_device_context = 0;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
{
    printf("\n\n\n--------------------this is running--------------------\n\n\n");
    Object3DList *object_3d_list = start_of_program(); // The initial

    float player_pos_x = 0, player_pos_y = 0, player_pos_z = -5;
    float player_rot_x = 0.1, player_rot_y = 0.2, player_rot_z = 0;
    // rot_x = player_up angle
    // rot_y = player_360 angle. Start looking in z direction. Turns counter clockwise toward +x direction. (I think)
    // rot_z = player_tilt angle
    Point3D player_rot_global = {.x = &player_rot_x, .y = &player_rot_y, .z = &player_rot_z};
    Point3D player_pos_global = {.x = &player_pos_x, .y = &player_pos_y, .z = &player_pos_z};

    const wchar_t window_class_name[] = L"My Window Class";
    static WNDCLASS window_class = {0};
    window_class.lpfnWndProc = WindowProcessMessage;
    window_class.hInstance = hInstance;
    window_class.lpszClassName = (PCSTR)window_class_name;
    RegisterClass(&window_class);

    /*
    frame_bitmap_info.bmiHeader.biSize = sizeof(frame_bitmap_info.bmiHeader);
    frame_bitmap_info.bmiHeader.biPlanes = 1;
    frame_bitmap_info.bmiHeader.biBitCount = 32;
    frame_bitmap_info.bmiHeader.biCompression = BI_RGB;
    frame_device_context = CreateCompatibleDC(0);
    */

    static HWND window_handle;
    window_handle = CreateWindow((PCSTR)window_class_name, "Drawing Pixels", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 window_top_left_x, window_top_left_y, window_width, window_height,
                                 NULL, NULL, hInstance, NULL);
    if (window_handle == NULL)
    {
        return -1;
    }

    while (!quit)
    {
        static MSG message = {0};
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&message);
        }

        // start of keyboard input code
        static int keyboard_x = 0, keyboard_y = 0;
        if (keyboard[VK_RIGHT] || keyboard['D'])
        {
            *(player_pos_global.x) += 0.001;
        }

        if (keyboard[VK_LEFT] || keyboard['A'])
        {
            *(player_pos_global.x) -= 0.001;
        }

        if (keyboard[VK_UP] || keyboard['W'])
        {
            *(player_pos_global.z) += (float)0.01;
        }

        if (keyboard[VK_DOWN] || keyboard['S'])
        {
            *(player_pos_global.z) -= (float)0.01;
        }

        if (keyboard[VK_SPACE])
        {
            *(player_pos_global.y) = 0.001;
        }

        *(player_rot_global.x) = (((float)(mouse.x)) * (float)(0.1));

        // end of keyboard input code

        draw_pixel(keyboard_x, keyboard_y, 0x00ffffff);

        draw_on_screen(object_3d_list, &player_pos_global, &player_rot_global);

        // frame.pixels[keyboard_x + keyboard_y*frame.width] = 0x00ffffff;

        InvalidateRect(window_handle, NULL, FALSE);
        UpdateWindow(window_handle);
    }

    return 0;
}

LRESULT CALLBACK WindowProcessMessage(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool has_focus = true;
    switch (message)
    {
    case WM_QUIT:
    case WM_DESTROY:
    {
        quit = true;
    }
    break;

    case WM_KILLFOCUS:
    {
        has_focus = false;
        memset(keyboard, 0, 256 * sizeof(keyboard[0]));
        mouse.buttons = 0;
    }
    break;

    case WM_SETFOCUS:
    {
        has_focus = true;
    }
    break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        if (has_focus)
        {
            static bool key_is_down, key_was_down;
            key_is_down = ((lParam & (1 << 31)) == 0);
            key_was_down = ((lParam & (1 << 30)) != 0);
            if (key_is_down != key_was_down)
            {
                keyboard[(uint8_t)wParam] = key_is_down;
                if (key_is_down)
                {
                    switch (wParam)
                    {
                    case VK_ESCAPE:
                        quit = true;
                        break;
                    }
                }
            }
        }
    }
    break;

    case WM_MOUSEMOVE:
    {
        mouse.x = LOWORD(lParam);
        mouse.y = frame.height - 1 - HIWORD(lParam);
    }
    break;

    // mouse buttons:
    case WM_LBUTTONDOWN:
        mouse.buttons |= MOUSE_LEFT;
        break;
    case WM_LBUTTONUP:
        mouse.buttons &= ~MOUSE_LEFT;
        break;
    case WM_MBUTTONDOWN:
        mouse.buttons |= MOUSE_MIDDLE;
        break;
    case WM_MBUTTONUP:
        mouse.buttons &= ~MOUSE_MIDDLE;
        break;
    case WM_RBUTTONDOWN:
        mouse.buttons |= MOUSE_RIGHT;
        break;
    case WM_RBUTTONUP:
        mouse.buttons &= ~MOUSE_RIGHT;
        break;

    case WM_XBUTTONDOWN:
    {
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
        {
            mouse.buttons |= MOUSE_X1;
        }
        else
        {
            mouse.buttons |= MOUSE_X2;
        }
    }
    break;
    case WM_XBUTTONUP:
    {
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
        {
            mouse.buttons &= ~MOUSE_X1;
        }
        else
        {
            mouse.buttons &= ~MOUSE_X2;
        }
    }
    break;

    case WM_MOUSEWHEEL:
    {
        printf("%s\n", wParam & 0b10000000000000000000000000000000 ? "Down" : "Up");
    }
    break;
        // end of mouse buttons

        /*
        case WM_SIZE:
        {
            //frame_bitmap_info.bmiHeader.biWidth = LOWORD(lParam);
            //frame_bitmap_info.bmiHeader.biHeight = HIWORD(lParam);

            if (frame_bitmap)
                DeleteObject(frame_bitmap);
            frame_bitmap = CreateDIBSection(NULL, &frame_bitmap_info, DIB_RGB_COLORS, (void *)&frame.pixels, 0, 0);
            SelectObject(frame_device_context, frame_bitmap);

            frame.width = LOWORD(lParam);
            frame.height = HIWORD(lParam);
        }
        break;
        */

        /*
         case WM_PAINT:
         {
             static PAINTSTRUCT paint;
             static HDC device_context;
             device_context = BeginPaint(window_handle, &paint);
             BitBlt(device_context,
                    paint.rcPaint.left, paint.rcPaint.top,
                    paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top,
                    frame_device_context,
                    paint.rcPaint.left, paint.rcPaint.top,
                    SRCCOPY);
             EndPaint(window_handle, &paint);
         }
         break;
         */

    default:
    {
        return DefWindowProc(window_handle, message, wParam, lParam);
    }
    }
    return 0;
}
