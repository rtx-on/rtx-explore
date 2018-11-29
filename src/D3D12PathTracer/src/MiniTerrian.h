#pragma once

/*
 * Size of chunk variables
 */
#define X_WIDTH 16
#define Y_WIDTH 256
#define Z_WIDTH 16

#define SIZE_OF_CHUNK X_WIDTH*Z_WIDTH*Y_WIDTH

#define CARVING_WIDTH 10.0f
#define CARVING_DEPTH 3.0f
#define CAVE_WIDTH 7.0f

#define DIAMOND_DEPOSIT_THRESHOLD 0.025f
#define GOLD_DEPOSIT_THRESHOLD 0.075f
#define REDSTONE_DEPOSIT_THRESHOLD 0.125f
#define IRON_DEPOSIT_THRESHOLD 0.1875f
#define COAL_DEPOSIT_THRESHOLD 1.25f

#define LAVA_CAVERN_CHANCE 0.1f

#define ORE_VEIN_RADIUS 3
#define CAVE_SPHERE_WIDTH 2
#define LARGE_CAVERN_SIZE 20

#define FEATURE_CREATE_THRESHOLD 0.2f

enum CarvingDirection : unsigned char {
    POSX, NEGX, POSZ, NEGZ
};

class MiniTerrain;

class Chunk : public Drawable
{
public:
    /*
     * Bottom left corner's first 32 bits are the x-coordinate and the second 32 bits are the z coordinate
     * These coordinates are **per-chunk**, as in, the chunk with bottom left corner at world space (x=36,z=36) would have
     * bottom left corner at (x=2, z=2).
     *
     * IMPORTANT INVARIANT: If you call this function, this Chunk must not already be in the map
     */
    Chunk(uint64_t key, MiniTerrain* terrian);

    /*
     * The populateChunk function calls MiniTerrain::CreateColumn in a loop to fill in the initial values
     * for this chunk's blocks using getBlockAt and setBlockAt
     */
    void populateChunk();

    /*
     * The create function creates the interleaved VBO for the chunk drawable object.
     * The VBO is made up of glm::vec4s in the order pos1nor1col1pos2nor2col2
     */
    void create(MiniBlock* mini_block);

    /*
     * The getBlockAtLocal and getBlockRefAtLocal functions take in local chunk coordinates and return the BlockType
     * or a reference to the BlockType at the location requested
     */
    BlockType getBlockAtLocal(int x, int y, int z) const;
    BlockType& getBlockRefAtLocal(int x, int y, int z);

    /*
     * Colors are assigned as following
     * DIRT = Brown (102, 51, 0)
     * STONE = Gray (128, 128, 128)
     * GRASS = Green (0, 204, 0)
     * LAVA = Red (255, 0, 0)
     */
    static glm::vec4 getBlockColor(BlockType b);
    void getFaceUVs(glm::vec2 topLeft, std::vector<glm::vec2> *uvs);
    glm::vec2 getTopLeftUV(BlockType b, glm::vec4 nor, std::vector<float> *pows);


    /*
     * helper function for create, adds the members to the VBO in interleaving fashion
     *
     * returns the new currentIndex
     */
    static int interleavingAddToVBOVector(glm::vec4 vecNor, glm::mat4 positionMat, glm::vec4 myColor, int currentIndex,
                                          std::vector<glm::vec4> &interleaved, std::vector<GLuint> &indices);

    uint64_t key; /* first 32-bits are x coord, 2nd 32 bits are z coord */
    bool toDraw = false;

private:
    BlockType blocks[SIZE_OF_CHUNK];  /* array is indexed using the formula i = x*Z_WIDTH*Y_WIDTH + z*Y_WIDTH + y */
    MiniTerrain* terrian;
};

class MiniTerrain
{
public:
    MiniTerrain();

    bool chunkExists(uint64_t key) {
        return this->chunkMap.find(key) != std::end(this->chunkMap);
    }

    void CreateColumn(glm::vec2 colPos);
    void fillColumn(BlockType b1, BlockType b2, BlockType b3, glm::vec2 colPos,
                             float colHeightPercent, float p1, float p2, float p3);

    /* This suite of functions facilitates the conversion of coordinates between the four spaces
     *
     * World Space - Relative to the origin of our scene
     * Chunk Local Space - Relative to the bottom left corner of the chunk the point is in
     * Chunk Coordinates - The coordinate of the chunk itself relative to the other chunks
     * Chunk Map Key - The key to index into the global map of chunks
     *
     * All of these functions take in glm::ivec3 for consistency but completely ignore the y coordinate
     * The function will either leave y coordinate unchanged or, if necessary, set it to 0
     */
    static uint64_t ConvertChunkToKey(glm::ivec3 chunkCoord);
    static glm::ivec3 ConvertKeyToChunk(uint64_t key);
    static glm::ivec3 ConvertWorldToChunk(glm::ivec3 world);
    static glm::ivec3 ConvertChunkToWorld(glm::ivec3 chunk);
    static glm::ivec3 ConvertWorldToChunkLocal(glm::ivec3 world);
    static glm::ivec3 ConvertChunkLocalToWorld(glm::ivec3 chunkLocal, const Chunk* chunk);

    BlockType getBlockAt(int x, int y, int z) const;   /* Given world-space coordinate return the block at that point */
    void setBlockAt(int x, int y, int z, BlockType t); /* Given world-space coordinate set the block at that point */

    /*
     * The getHeightOfMiniTerrainAtColumn function returns the y-coordinate of the last grass block in the column
     * (passed in world space coordinates)
     */
    int getHeightOfTerrainAtColumn(int x, int z) const;

    /*
     * The generateRiver function generates a river at the given x and z coordinates
     * with the given orientation and RiverType
     */
    void generateRiver(RiverType type, glm::vec3 position, glm::vec3 orientation);

    /*
     * The generateRiver function generates a river at the given x and z coordinates
     * with the given orientation and RiverType
     */
    void generateCave(glm::vec3 position, glm::vec3 orientation);

    /*
     * The generateBiomeSpecificMiniTerrainFeature function generates
     *  - For GRASS biomes : a Tree
     *  - For SAND biomes : a cactus
     *  - For SNOW biomes : an icicle
     *  - For LAVA biomes : an island
     */
    void generateBiomeSpecificTerrainFeature(glm::ivec3 position);

    /*
     * These functions generate the biome specific features
     */
    void generateTree(glm::ivec3 position);
    void generateIcicle(glm::ivec3 position);
    void generateIsland(glm::ivec3 position);
    void generateCactus(glm::ivec3 position);

    /*
     * The carve river at line function takes in a pair of glm::vec3s and creates a river between them
     */
    void carveRiverAtLine(std::pair<glm::vec3, glm::vec3> line, int yValue);

    void carveCaveAtLine(std::pair<glm::vec3, glm::vec3> line);

    void carveAroundPoint(glm::vec3 point, glm::vec3 riverRightVector, glm::vec3 riverDirection, int width, int depth);

    void carveSphereAroundPoint(glm::vec3 center, int radius, bool isLavaCavern);

    void carveLargeLavaCavern(glm::vec3 point);

    void maybeGenerateOreVeinAtPoint(glm::vec3 point);

    void clearTerrainAboveBlock(int x, int y, int z);

    void carveSlopeInDirection(int x, int y, int z, glm::vec3 dir);

    void designateChunkAsToDraw(int x, int y, int z);

    glm::ivec3 dimensions; /* a vector of dimensions of the chunk */
    std::map<uint64_t, Chunk*> chunkMap; /* a map of all chunks that have ever been created, indexed by a 64bit unique key */
    float continuity = 200.0f; /* the degree of continuity for the procedural MiniTerrain generation */
};
