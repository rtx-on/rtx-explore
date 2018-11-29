#pragma once

#define MAX_RECURSION_DEPTH 5
#define WAVINESS 0.40f
#define FORWARD_DISTANCE 30.0f
#define RECURSION_DROP_FACTOR 1.0f

#define CAVE_MAX_RECURSION_DEPTH 12
#define CAVE_WAVINESS 0.40f
#define CAVE_FORWARD_DISTANCE 8.0f
#define CAVE_RECURSION_DROP_FACTOR 1.0f

enum RiverType : unsigned char { LINEAR, DELTA, CAVE };

class MiniTurtle;

typedef void (*Rule)(MiniTurtle*);

/* MiniLSystem Class overview:
 *      - defines all of the rules for turtle movement
 *      - constructor takes in the RiverType and the initial position/orientation
 *          - constructor gets the axiom and the maps/grammars (hardcoded from the RiverType argument)
 *          - constructor creates the first turtle and initializes it with the axiom and the lsystem initial position
 *              then pushes the MiniTurtle onto the QStack
 *
 *      - has 3 member variables:
 *          map of character to QString
 *          map of character to rule
 *          Qstack of turtles
 *
 *       exposes one function
 *       getNextLine
 *         - stores the current top turtle's position
 *         - calls executeNextMovementRule in a while loop (while return value != NULL) and stack != empty
 *
 *       Rule functions should be statically defined by the MiniLSystem class and take in MiniTurtle* MiniLSystem* and return void
 */

class MiniLSystem
{
public:
    MiniLSystem(RiverType type, glm::vec3 position, glm::vec3 orientation);

    /**
     * @brief getNextLine uses the MiniLSystem to generate the next line to draw
     * @return a pair of position vectors corresponding to the points to draw, NULL if there is none
     */
    std::pair<glm::vec3, glm::vec3> getNextLine();

    /**
     * The following is a suite of rules for the Turtles to execute:
     *
     * All movement/rotation has an initial distance and is scaled by a factor of 1/turtle->recursionDepth
     */
    static void ruleMoveForward(MiniTurtle* turtle);
    static void ruleTurnLeft(MiniTurtle* turtle);
    static void ruleTurnRight(MiniTurtle* turtle);
    static void ruleTurnUp(MiniTurtle* turtle); /* turtles gettin turnt */
    static void ruleTurnDown(MiniTurtle* turtle); /* for what? */
    static void ruleTurnAround(MiniTurtle* turtle);
    static void ruleRecurse(MiniTurtle* turtle);
    static void rulePop(MiniTurtle* turtle);

    int getCurrentRecursionDepth();

    std::stack<MiniTurtle*> turtleStack;
    std::map<char, std::string> charToGrammarExpansion;
    std::map<char, Rule> charToDrawingOperation;
    int maxRecursionDepth;
    int recursionDropFactor;

private:
    /**
     * @brief initLSystemMaps initializes the MiniLSystem Maps to their defaults based on the RiverType
     */
    void initLSystemMaps(RiverType type);

    /**
     * @brief getInitialAxiom returns the hardcoded initial axiom for the RiverType
     * @param type the RiverType
     * @return A QString denoting the axiom string
     */
    static std::string getInitialAxiom(RiverType type);

    /**
     * @brief populateLinearGrammarExpansion populates the Grammar expansion map with the correct expansions
     *        for the LINEAR RiverType (i.e. 'X' -> '[-FX]+FX')
     */
    void populateGrammarMapLinear();

    /**
     * @brief populateDeltaGrammarExpansion populates the Grammar expansion map with the correct expansions
     *        for the LINEAR RiverType (i.e. 'X' -> '[-FX]+FX')
     */
    void populateRuleMapLinear();

    /**
     * @brief populateLinearDrawingOperations populates the drawing operations map with the correct rules
     *        for the DELTA RiverType (i.e. 'F' -> moveAndDrawLine)
     */
    void populateGrammarMapDelta();

    /**
     * @brief populateDeltaDrawingOperations populates the drawing operations map with the correct rules
     *        for the DELTA RiverType (i.e. 'F' -> moveAndDrawLine)
     */
    void populateRuleMapDelta();

    /**
     * @brief populateLinearDrawingOperations populates the drawing operations map with the correct rules
     *        for the CAVE RiverType (i.e. 'F' -> moveAndDrawLine)
     */
    void populateGrammarMapCave();

    /**
     * @brief populateDeltaDrawingOperations populates the drawing operations map with the correct rules
     *        for the CAVE RiverType (i.e. 'F' -> moveAndDrawLine)
     */
    void populateRuleMapCave();
};


