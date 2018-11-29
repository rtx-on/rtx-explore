#pragma once
class MiniLSystem;

class MiniTurtle
{
public:
    /**
     * @brief MiniTurtle clone constructor. Takes old turtle's position/orientation and increments recursionDepth
     * @param turtle the turtle we will be copying from
     * @param newGrammar the new translated grammar
     */
    MiniTurtle(MiniTurtle* turtle, std::string newGrammar);

    /**
     * @brief MiniTurtle clone constructor. Takes position/orientation and increments recursionDepth
     * @param turtle the turtle we will be copying from
     * @param newIter the new position of the iterator
     */
    MiniTurtle(MiniTurtle* turtle, std::string::iterator newIter);

    /**
     * @brief MiniTurtle initial constructor (for the first turtle)
     * @param initialAxiom the starting axiom of the LSystem
     * @param system the LSystem that the turtle is a part of (used for position/orientation/etc.)
     * @param position the initial position of the LSystem
     * @param orientation the initial orientation of the LSystem
     */
    MiniTurtle(std::string initialAxiom, MiniLSystem* system, glm::vec3 position, glm::vec3 orientation);

    /**
     * @brief getMyPosition returns the MiniTurtle's position
     * @return a glm::vec3 denoting the position
     */
    glm::vec3 getMyPosition();

    /**
     * @brief executeNextMovementRule continues to read the characters until it executes a movement rule or gets popped off the stack
     * @param rule the rule you wish to execute
     * @return empty pair if we were removed from the stack or a pair of glm::vec3s denoting our next movement
     */
    std::pair<glm::vec3, glm::vec3> executeNextMovementRule();

    glm::vec3 position;
    glm::vec3 orientation;
    int recursionDepth;
    MiniLSystem* system;
    std::string::iterator iter;
    std::string grammar;
};

