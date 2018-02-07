#if !defined ENGINE_MATH_H
/* ============================================================================
   $File: $engine_math.h
   $Date: $02-02-2018
   $Revision: $
   $Creator: Jacob Kartchner $
   $Notice: (c) Copyright 2018 by Jacob Kartchner. All Rights Reserved. $
   =========================================================================== */

// Math can improve our language of the game mechanics in the same way
// C improved our ability to abstract and understand our code


/* VECTORS ------------------------------------------------------------
 * 
 * Foundation of vector math is the pythagorean theorem. 
 * TODO: Add in ep42 writings on diagonal movement, reduced equation
 *
 * Will use operator overloading here to improve code readability later.
 * Did not create overload for operators *=, +=, -= -- see HH ep 42
 *---------------------------------------------------------------------*/

typedef struct v2 {  
    union           // union allows us to initialize as v2 A = {1, 2};
    {               //                                  v2 A = {}; (init to 0)
       struct       //                                  v2 A = {1}; (2nd var is 0)
        {           //        and handle vars as A.x + 2; or A[0] + 2, etc.
            float x, y;
        };
        float E[2];
    };

    inline v2 &operator*=(float A);
    inline v2 &operator+=(v2 A);

} v2;              


inline v2 operator+(v2 A, v2 B) // A + B = result
{
    v2 result;

    result.x = A.x + B.x;
    result.y = A.y + B.y;

    return result;
}

inline v2 operator-(v2 A, v2 B) // A - B = result
{
    v2 result;

    result.x = A.x - B.x;
    result.y = A.y - B.y;

    return result;
}


inline v2 operator-(v2 A) // B = -A
{
    v2 result;

    result.x = -A.x;
    result.y = -A.y;

    return result;
}

inline v2 operator*(float A, v2 B) // float A * v2 B = v2 C
{
    v2 result;

    result.x = A * B.x;
    result.y = A * B.y;

    return result;
}


inline v2 &v2::operator*=(float A) // v2 A *= float B
{
    *this = A * *this;

    return *this;
}


inline v2 &v2::operator+=(v2 A) // v2 A += v2;
{
    *this = A + *this;

    return *this;
}



inline v2 V2(float X, float Y) // this is annoying; b/c of the union, 
{                              // can't initialize like v2 a = { 1, 2 };
    v2 result;                 // So made a function to do it
                               
    result.x = X;
    result.y = Y;

    return result;
}

typedef struct Color {
    float red;
    float green;
    float blue;
    float alpha;
} Color;

internal uint32_t
RoundFloatToInt32(float Float)
{
    int32_t result = (int32_t)(Float + 0.5f);
    return result;
}

internal uint32_t
GetColorInt(Color color)
{
    unsigned char red = RoundFloatToInt32(color.red * 255);
    unsigned char green = RoundFloatToInt32(color.green * 255);
    unsigned char blue = RoundFloatToInt32(color.blue * 255);
    unsigned char alpha = RoundFloatToInt32(color.alpha * 255);
    return alpha << 24 | red << 16 | green << 8 | blue;
}

internal Color
GetColor(float a, float r, float g, float b)
{
    Color color = { a, r, g, b }; 
    return color;
}

/* EQUATIONS OF MOTION ------------------------------------------------
 * 
 * HH Ep43
 * SO MANY GAMES, especially indy games, feel awful to control. It's 
 * because they just made up the controls. They tuned it. There's 
 * no visceral feel to it. Having the power of full physics with you
 * yields a game that feels so much more natural. It's subtle, but 
 * makes a BIG difference
 *
 * Rigid Body Dynamics - rigid body dynamics speaks to the way in which matter in 
 * the world moves when you consider it as a rigid, unmoving body - planets, balls
 *      linear - how is something moving through space - a player walking
 *      angular - how is something rotating in place
 *
 * Linear Rigid Body Dynamics
 *  Most of any complexity here can be reduced to one thing: the 
 *  center of mass. This area is useful in 3d; less so in a 2d game
 *  Gravity, momentum, etc. are forces acting on the center of mass.
 *  f = ma  (force equals mass times acceleration)
 *  To solve for a = f / m
 *  The heavier our object is, the less it will move. We need to figure
 *  out a way to relate our solution to a to the real world. All depend on time:
 *    Position f(t) - we already have this as a given point in a vector at a 
 *                    given frame of the game
 *    Velocity f1(t) - first derivative of position (meters per second or speed)
 *    Acceleration f11(t) - second derivative of position
 *   
 *    ALL 3 ARE CALCULATED PER FRAME 
 *      vectors should first calculate position and direction 
 *      and then multiply by a magnitude of velocity (m/s)
 *
 * How it was initially done in the code
 *     We were kind of already doing most of the movement equation by 
 *     computing a direction and then multiplying by a velocity. 
 *     We just would have needed to add acceleration factor:
 *         1/2 Acceleration * time squared
 *
 * How we want to change it with our new equation
 *     In the end we'll tweak most of the variables anyway. But acceleration
 *     is just input from the player. Usually accelerates as X meters per sec squared!
 *     When casey implements it, it's literally one or two lines of code to 
 *     get acceleration of movement. However, this equation alone produces
 *     an object that behaves like he's on ice. This is consistent with law of
 *     physics. We need to implement friction. 
 *
 * deltaTime (Dt) - treat these functions as if they exist only for 
 * one given frame. In other words, time is the delta of the time since
 * the last frame. 
 * 
 * MAX SPEED - is an artificial construct. Nothing is stopping us
 * from going speed of light other than environmental variables. Set
 * max speed by the environment the movement is in (e.g., friction)
 * rather than some max speed clamp
 * -------------------------------------------------------------------*/


#define ENGINE_MATH_H
#endif
