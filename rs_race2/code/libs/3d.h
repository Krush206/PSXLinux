// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// Three d model header file
// ***********************************************************************************

// **** Global defines
#define	LINE_setVERTEX(v,_x,_y,_z) \
				(v)->vec.vx = (_x), \
				(v)->vec.vy = (_y), \
				(v)->vec.vz = (_z)
#define	LINE_setENDS(l,_s,_e) \
				(l)->start = (_s), \
				(l)->end = (_e)

#define   SetLightDirectional(p,_x,_y,_z,_r,_g,_b) \
            LightSource[(p)].vx = (_x), \
            LightSource[(p)].vy = (_y), \
            LightSource[(p)].vz = (_z), \
            LightSource[(p)].r = (_r), \
            LightSource[(p)].g = (_g), \
            LightSource[(p)].b = (_b), \
            GsSetFlatLight((p), &LightSource[(p)])

#define   SetLightAmbient(_r,_g,_b) \
            GsSetAmbient((_r),(_g),(_b))

#define   TurnFoggingOn() \
            GsSetLightMode(1)

#define   TurnFoggingOff() \
            GsSetLightMode(0)

#define   MoveModelTo(p,_x,_y,_z) \
            (p)->Object_Coord.coord.t[0] = (_x), \
            (p)->Object_Coord.coord.t[1] = (_y), \
            (p)->Object_Coord.coord.t[2] = (_z), \
            (p)->Object_Coord.flg = 0

#define   MoveModelBy(p,_x,_y,_z) \
            (p)->Object_Coord.coord.t[0] += (_x), \
            (p)->Object_Coord.coord.t[1] += (_y), \
            (p)->Object_Coord.coord.t[2] += (_z), \
            (p)->Object_Coord.flg = 0

#define   SetModelSubdivision(p,_sd) \
            (p)->Object_Handler.attribute += -((p)->Subdivision) + (_sd), \
            (p)->Subdivision = (_sd)

// **** Global variables
typedef struct
  {
  u_long            Subdivision;
  SVECTOR           Orientation;
  GsDOBJ2           Object_Handler;
  GsCOORDINATE2     Object_Coord;
  } Model;

GsF_LIGHT     LightSource[3];

GsRVIEW2      ViewPoint;

// ***********************************************************************************
// Function prototypes
// ***********************************************************************************

// **** Set up a model
void SetModelInfo(Model *tModel, long tX, long tY, long tZ, u_long *MemAddress);

// **** Set viewpoint for scene
void SetViewPoint(long tProjDist, long tRz,
      long tFromX, long tFromY, long tFromZ,
      long tToX, long tToY, long tToZ,
      GsCOORDINATE2 *tReference);

// **** Draw the model
void DrawModel(Model *tModel, long tBitShift);

// **** Rotate Model
void RotateModel(Model *tModel, long tXr, long tYr, long tZr);

// **** Translate an object according to the direction it is facing
void MoveModelForward(Model *tModel, long tX, long tY, long tZ);

