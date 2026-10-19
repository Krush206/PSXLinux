// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// MAIN routine
// ***********************************************************************************

// **** TRACK pieces definition
#define   TRK_4_S_S       (0)//
#define   TRK_4_S_L8      (1)//
#define   TRK_4_S_L32     (2)//
#define   TRK_4_L8_S      (3)//
#define   TRK_4_L8_L8     (4)//
#define   TRK_4_L8_L16    (5)//
#define		TRK_4_L16_S			(6)
#define   TRK_4_L16_L8    (7)//
#define   TRK_4_L16_L16   (8)//
#define   TRK_4_L32_S     (9)//
#define   TRK_4_L32_L8    (10)//
#define		TRK_4_L32_L16		(11)//
#define   TRK_4_L32_L32   (12)//
#define   TRK_4_L32_L64   (13)//
#define   TRK_4_L64_L32   (14)//
#define   TRK_4_L64_L64   (15)//
#define   TRK_4_S_R8      (16)//
#define   TRK_4_S_R64     (17)//
#define   TRK_4_R8_S      (18)//
#define   TRK_4_R8_R8     (19)//
#define   TRK_4_R8_R20    (20)//
#define   TRK_4_R20_R8    (21)//
#define   TRK_4_R20_R20   (22)//
#define   TRK_4_R20_R48   (23)//
#define   TRK_4_R48_R8    (24)//
#define   TRK_4_R48_R48   (25)//
#define   TRK_4_R64_S     (26)//
#define   TRK_4_R64_R64   (27)//
#define   TRK_4_BRIDGE    (28)//
#define   TRK_4_BRIDGE2   (29)//
#define   TRK_4_TUNNEL    (30)//
#define   TRK_4_TUNNELIN 	(31)//
#define   TRK_4_TUNNELIN2 (32)//
#define   TRK_4_TUNNEL2   (33)//
#define   TRK_4_TUNNELOUT (34)//
#define   TRK_4_TUNNELOUT2 (35)//
#define		TRK_2L_S_R32		(36)
#define		TRK_2L_R32_R32	(37)
#define		TRK_2L_R32_S		(38)
#define		TRK_2L_S_S			(39)
#define		TRK_2L_S_L16		(40)
#define		TRK_2L_L16_L16	(41)
#define		TRK_2L_L16_L64	(42)
#define		TRK_2R_S_L32		(43)
#define		TRK_2R_L32_L32	(44)
#define		TRK_2R_L32_S		(45)
#define		TRK_2R_S_S			(46)
#define		TRK_2R_S_R32		(47)
#define		TRK_2R_R32_R32	(48)
#define		TRK_2R_R32_S		(49)
#define		TRK_2R_S_S_START	(50)
#define		TRK_2R_S_S_END		(52)
#define		TRK_2L_S_S_START		(53)
#define		TRK_2L_S_S_END		(54)

#define		NORMAL 					(0)
#define		LEFT						(1)
#define		RIGHT						(2)

long tCurrentOrientation;

void create_Track();

void setpoint(VERTEX *tVertex, long tAngle, long tX, long tY, long tZ);

void track_2L_front(VERTEX *tVertex, long tAngle);
void track_2L_rear(VERTEX *tVertex, long tAngle);
void track_2R_front(VERTEX *tVertex, long tAngle);
void track_2R_rear(VERTEX *tVertex, long tAngle);

void track_4_front(VERTEX *tVertex, long tAngle);
void track_4_rear(VERTEX *tVertex, long tAngle);

void create_TRK_4_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng);
void create_TRK_2L_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng);
void create_TRK_2L_START_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng);
void create_TRK_2L_END_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng);
void create_TRK_2R_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng);
void create_TRK_2R_START_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng);
void create_TRK_2R_END_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng);

void create_TRK_4_BRIDGE();
void create_TRK_4_BRIDGE2();
void create_TRK_4_TUNNEL();
void create_TRK_4_TUNNELIN();
void create_TRK_4_TUNNELIN2();
void create_TRK_4_TUNNEL2();
void create_TRK_4_TUNNELOUT();
void create_TRK_4_TUNNELOUT2();