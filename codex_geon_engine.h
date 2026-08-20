/**
 * @file codex_geon_engine.h
 * @brief Micro Polyhedral Geometry Engine & Scene Map List for Codex OS Project.
 * 
 * Optimized for minimalist OpenGL buffer objects (Vertex + Normal interleaved 
 * or sequential streams) and extremely low-resolution volumetric primitives.
 * 
 * Geon Primitives Included:
 *  1. HEXAHEDRON (Cube)             -> Stoves, computer consoles, rectangular cabinets.
 *  2. TRIANGULAR_PRISM (Wedge)      -> Classic telephones, terminal slopes, ramps.
 *  3. HEXAGONAL_PRISM (Cylinder)   -> Coke bottles, barrels, pipes, dials.
 *  4. RHOMBIC_DODECAHEDRON          -> Dense space-filling clusters, blobby debris.
 */

#ifndef CODEX_GEON_ENGINE_H
#define CODEX_GEON_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <math.h>

/* ========================================================================== */
/*                          MICRO GAME ENGINE STRUCTURES                      */
/* ========================================================================== */

/**
 * @brief Enumeration of the core lite polyhedral types.
 */
typedef enum {
    GEON_HEXAHEDRON = 0,
    GEON_WEDGE_PRISM,
    GEON_HEXAGONAL_PRISM,
    GEON_RHOMBIC_DODECAHEDRON,
    GEON_TYPE_COUNT
} GeonType;

/**
 * @brief Tiny vertex format matching standard micro-OpenGL layouts.
 */
typedef struct {
    float x, y, z;    /* Position coordinates */
    float nx, ny, nz;  /* Surface Normals (Crucial for low-res faceted lighting) */
} GeonVertex;

/**
 * @brief Polyhedral geometry descriptor for populating OpenGL VBOs/IBOs.
 */
typedef struct {
    const GeonVertex* vertices;
    uint32_t vertex_count;
    const uint16_t* indices;
    uint32_t index_count;
} GeonMeshBuffer;

/**
 * @brief Transform node for the built-in micro development kit scene map list.
 * Pack tightly to keep kernel allocation structures light.
 */
typedef struct SceneNode {
    uint32_t id;            /* Unique primitive identifier */
    uint8_t geon_type;      /* Type from GeonType enum */
    uint8_t flags;          /* For engine state (e.g., active, static, transparent) */
    uint16_t reserved;      /* Explicit 32-bit padding alignment */
    
    /* Spatial Map Data */
    float x, y, z;          /* Position coordinates */
    float rot_x, rot_y, rot_z; /* Rotation configuration (Euler angles in radians) */
    float scale_x, scale_y, scale_z; /* Dimensional scaling factors */
    
    struct SceneNode* next; /* Linked list pointer for the active scene map stream */
} SceneNode;


/* ========================================================================== */
/*                      STATIC POLYHEDRAL GEOMETRY DATA                       */
/* ========================================================================== */

/* 1. HEXAHEDRON (Cube) - Ideal for Stoves, Desktop Terminals */
static const GeonVertex HEXAHEDRON_VERTICES[] = {
    /* Front Face */
    {-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f}, { 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f},
    { 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f}, {-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f},
    /* Back Face */
    {-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f}, {-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f},
    { 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f}, { 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f},
    /* Top Face */
    {-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f}, {-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f},
    { 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f}, { 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f},
    /* Bottom Face */
    {-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f}, { 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f},
    { 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f}, {-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f},
    /* Right Face */
    { 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f}, { 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f},
    { 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f}, { 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f},
    /* Left Face */
    {-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f}, {-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f},
    {-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f}, {-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f}
};
static const uint16_t HEXAHEDRON_INDICES[] = {
    0, 1, 2,    0, 2, 3,       /* Front */
    4, 5, 6,    4, 6, 7,       /* Back */
    8, 9, 10,   8, 10,11,      /* Top */
    12,13,14,   12,14,15,      /* Bottom */
    16,17,18,   16,18,19,      /* Right */
    20,21,22,   20,22,23       /* Left */
};

/* 2. TRIANGULAR PRISM (Wedge) - Ideal for Classical Slanted Telephones, Monitors */
static const GeonVertex WEDGE_VERTICES[] = {
    /* Base Face (Bottom) */
    {-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f}, { 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f},
    { 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f}, {-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f},
    /* Vertical Back Face */
    {-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f}, {-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f},
    { 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f}, { 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f},
    /* Slanted Ramp Face */
    {-0.5f,  0.5f, -0.5f,  0.0f,  0.7071f, 0.7071f}, { 0.5f,  0.5f, -0.5f,  0.0f,  0.7071f, 0.7071f},
    { 0.5f, -0.5f,  0.5f,  0.0f,  0.7071f, 0.7071f}, {-0.5f, -0.5f,  0.5f,  0.0f,  0.7071f, 0.7071f},
    /* Left Profile Triangle */
    {-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f}, {-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f},
    {-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f},
    /* Right Profile Triangle */
    { 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f}, { 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f},
    { 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f}
};
static const uint16_t WEDGE_INDICES[] = {
    0, 2, 1,    0, 3, 2,       /* Base */
    4, 5, 6,    4, 6, 7,       /* Back */
    8, 9, 10,   8, 10,11,      /* Ramp */
    12,13,14,                  /* Left flat */
    15,16,17                   /* Right flat */
};

/* 3. HEXAGONAL PRISM - High efficiency approximation for Bottles, Valves, Pipes */
static const GeonVertex HEX_PRISM_VERTICES[] = {
    /* Top Hex Cap (Normal pointing up: 0,1,0) */
    { 0.0f,   0.5f, -0.5f,   0.0f, 1.0f, 0.0f}, { 0.433f, 0.5f, -0.25f,  0.0f, 1.0f, 0.0f},
    { 0.433f, 0.5f,  0.25f,  0.0f, 1.0f, 0.0f}, { 0.0f,   0.5f,  0.5f,   0.0f, 1.0f, 0.0f},
    {-0.433f, 0.5f,  0.25f,  0.0f, 1.0f, 0.0f}, {-0.433f, 0.5f, -0.25f,  0.0f, 1.0f, 0.0f},
    /* Bottom Hex Cap (Normal pointing down: 0,-1,0) */
    { 0.0f,  -0.5f, -0.5f,   0.0f,-1.0f, 0.0f}, {-0.433f,-0.5f, -0.25f,  0.0f,-1.0f, 0.0f},
    {-0.433f,-0.5f,  0.25f,  0.0f,-1.0f, 0.0f}, { 0.0f,  -0.5f,  0.5f,   0.0f,-1.0f, 0.0f},
    { 0.433f,-0.5f,  0.25f,  0.0f,-1.0f, 0.0f}, { 0.433f,-0.5f, -0.25f,  0.0f,-1.0f, 0.0f},
    
    /* Side quad slices require split vertices for hard facet normals */
    /* Slice 0 (Front-Right) */
    { 0.0f,   0.5f, -0.5f,   0.866f, 0.0f, -0.5f}, { 0.433f, 0.5f, -0.25f,  0.866f, 0.0f, -0.5f},
    { 0.433f,-0.5f, -0.25f,  0.866f, 0.0f, -0.5f}, { 0.0f,  -0.5f, -0.5f,   0.866f, 0.0f, -0.5f},
    /* Slice 1 (Far Right) */
    { 0.433f, 0.5f, -0.25f,  0.866f, 0.0f,  0.5f}, { 0.433f, 0.5f,  0.25f,  0.866f, 0.0f,  0.5f},
    { 0.433f,-0.5f,  0.25f,  0.866f, 0.0f,  0.5f}, { 0.433f,-0.5f, -0.25f,  0.866f, 0.0f,  0.5f}
    /* Remaining side slices can be similarly expanded to preserve strict hardware normals */
};
static const uint16_t HEX_PRISM_INDICES[] = {
    0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 5, /* Top Cap Fan */
    6, 7, 8,    6, 8, 9,    6, 9, 10,   6, 10,11, /* Bottom Cap Fan */
    12,13,14,   12,14,15,                         /* Side 0 */
    16,17,18,   16,18,19                          /* Side 1 */
};

/* 4. RHOMBIC DODECAHEDRON - Space-filling cellular module for organic/irregular piles */
static const GeonVertex RHOMBIC_VERTICES[] = {
    /* Core uniform coordinate distribution representing the dual space-filler */
    { 0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f}, { 0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f},
    { 1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f}, {-1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f},
    { 0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f}, { 0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f},
    { 0.5f,  0.5f,  0.5f,  0.577f, 0.577f, 0.577f}, {-0.5f,  0.5f,  0.5f, -0.577f, 0.577f, 0.577f},
    { 0.5f, -0.5f,  0.5f,  0.577f,-0.577f, 0.577f}, {-0.5f, -0.5f,  0.5f, -0.577f,-0.577f, 0.577f},
    { 0.5f,  0.5f, -0.5f,  0.577f, 0.577f,-0.577f}, {-0.5f,  0.5f, -0.5f, -0.577f, 0.577f,-0.577f},
    { 0.5f, -0.5f, -0.5f,  0.577f,-0.577f,-0.577f}, {-0.5f, -0.5f, -0.5f, -0.577f,-0.577f,-0.577f}
};
static const uint16_t RHOMBIC_INDICES[] = {
    0, 6, 2,    0, 2, 8,   /* Rhombus face 1 */
    0, 7, 3,    0, 3, 9,   /* Rhombus face 2 */
    0, 6, 4,    0, 4, 7,   /* Rhombus face 3 */
    0, 8, 5,    0, 5, 9,   /* Rhombus face 4 */
    1,10, 2,    1, 2,12,   /* Rhombus face 5 */
    1,11, 3,    1, 3,13    /* Rhombus face 6 */
};


/* ========================================================================== */
/*                          ENGINE PIPELINE INTERFACE                         */
/* ========================================================================== */

/**
 * @brief Fetches standard read-only geometric bindings to populate an OpenGL VBO.
 * 
 * @param type Target polyhedral volumetric primitive.
 * @return GeonMeshBuffer Configuration tracking structures.
 */
static inline GeonMeshBuffer codex_get_geon_mesh(GeonType type) {
    GeonMeshBuffer mesh = { nullptr, 0, nullptr, 0 };
    switch (type) {
        case GEON_HEXAHEDRON:
            mesh.vertices = HEXAHEDRON_VERTICES;
            mesh.vertex_count = sizeof(HEXAHEDRON_VERTICES) / sizeof(GeonVertex);
            mesh.indices = HEXAHEDRON_INDICES;
            mesh.index_count = sizeof(HEXAHEDRON_INDICES) / sizeof(uint16_t);
            break;
        case GEON_WEDGE_PRISM:
            mesh.vertices = WEDGE_VERTICES;
            mesh.vertex_count = sizeof(WEDGE_VERTICES) / sizeof(GeonVertex);
            mesh.indices = WEDGE_INDICES;
            mesh.index_count = sizeof(WEDGE_INDICES) / sizeof(uint16_t);
            break;
        case GEON_HEXAGONAL_PRISM:
            mesh.vertices = HEX_PRISM_VERTICES;
            mesh.vertex_count = sizeof(HEX_PRISM_VERTICES) / sizeof(GeonVertex);
            mesh.indices = HEX_PRISM_INDICES;
            mesh.index_count = sizeof(HEX_PRISM_INDICES) / sizeof(uint16_t);
            break;
        case GEON_RHOMBIC_DODECAHEDRON:
            mesh.vertices = RHOMBIC_VERTICES;
            mesh.vertex_count = sizeof(RHOMBIC_VERTICES) / sizeof(GeonVertex);
            mesh.indices = RHOMBIC_INDICES;
            mesh.index_count = sizeof(RHOMBIC_INDICES) / sizeof(uint16_t);
            break;
        default:
            break;
    }
    return mesh;
}

/**
 * @brief Linear search matrix to find an object mapping by geographic position.
 * Useful for the built-in micro-dev kit editor tracking tool.
 */
static inline SceneNode* codex_find_node_at(SceneNode* head, float x, float y, float z, float epsilon) {
    SceneNode* current = head;
    while (current != NULL) {
        if (fabsf(current->x - x) <= epsilon &&
            fabsf(current->y - y) <= epsilon &&
            fabsf(current->z - z) <= epsilon) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* CODEX_GEON_ENGINE_H */

