#include "header.h"

// ***********************************************************************************
// MAIN FUNCTION
// ***********************************************************************************

void create_RoadsideObjects()
  {
  create_OBJECT_TUNNEL_ENTRANCE();
  }

// ***********************************************************************************
// DYNAMIC TMD CREATION for all roadside objects
// ***********************************************************************************

void create_YourCar()
  {
  VERTEX  v[8];
  COLOUR  rgb[8];

  TMD_setVERTEX(&v[0], -50, -100, -50);
  TMD_setVERTEX(&v[1], -50, -100, 50);
  TMD_setVERTEX(&v[2], 50, -100, 50);
  TMD_setVERTEX(&v[3], 50, -100, -50);
  TMD_setVERTEX(&v[4], -50, 0, -50);
  TMD_setVERTEX(&v[5], -50, 0, 50);
  TMD_setVERTEX(&v[6], 50, 0, 50);
  TMD_setVERTEX(&v[7], 50, 0, -50);
  
  TMD_setCOLOUR(&rgb[0], 0, 0, 0);
  TMD_setCOLOUR(&rgb[1], 255, 0, 0);
  TMD_setCOLOUR(&rgb[2], 255, 255, 0);
  TMD_setCOLOUR(&rgb[3], 0, 255, 0);
  TMD_setCOLOUR(&rgb[4], 0, 0, 255);
  TMD_setCOLOUR(&rgb[5], 255, 0, 255);
  TMD_setCOLOUR(&rgb[6], 255, 255, 255);
  TMD_setCOLOUR(&rgb[7], 0, 255, 255);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 0, 1, 3, 2, 0, 1, 2, 3, false);
  TMD_set4_NS_GP( 1, 0, 5, 4, 1, 0, 4, 5, false);
  TMD_set4_NS_GP( 3, 2, 7, 6, 3, 2, 6, 7, false);
  TMD_set4_NS_GP( 0, 3, 4, 7, 0, 3, 7, 4, false);
  TMD_set4_NS_GP( 2, 1, 6, 5, 2, 1, 5, 6, false);
  TMD_set4_NS_GP( 4, 7, 5, 6, 4, 7, 6, 5, false);

  TMD_YOURCAR = TMD_finish();
  }

void create_OBJECT_TUNNEL_ENTRANCE()
  {
  VERTEX  v[42];
  COLOUR  rgb[10];

  TMD_setVERTEX(&v[0], -500, -100, -105);
  TMD_setVERTEX(&v[1], -400, 0, -105);
  TMD_setVERTEX(&v[2], -248, 204, -105);
  TMD_setVERTEX(&v[4], 246, 208, -105);
  TMD_setVERTEX(&v[5], 400, 0, -105);
  TMD_setVERTEX(&v[6], 500, -100, -105);
  TMD_setVERTEX(&v[7], -450, -300, -105);
  TMD_setVERTEX(&v[8], -400, -350, -105);
  TMD_setVERTEX(&v[9], -200, -400, -105);
  TMD_setVERTEX(&v[10], 0, -400, -105);
  TMD_setVERTEX(&v[11], 200, -400, -105);
  TMD_setVERTEX(&v[12], 400, -350, -105);
  TMD_setVERTEX(&v[13], 450, -300, -105);
  TMD_setVERTEX(&v[14], -364, -620, 69);
  TMD_setVERTEX(&v[15], -110, -552, -29);
  TMD_setVERTEX(&v[16], 152, -670, 127);
  TMD_setVERTEX(&v[17], 430, -586, -113);
  TMD_setVERTEX(&v[18], 364, -838, 185);
  TMD_setVERTEX(&v[19], 616, -472, -3);
  TMD_setVERTEX(&v[20], 574, -730, 187);
  TMD_setVERTEX(&v[21], -620, -466, 105);
	TMD_setVERTEX(&v[22], -148, -794, 323);
  TMD_setVERTEX(&v[23], -756, -246, -183);
  TMD_setVERTEX(&v[24], -902, -514, -61);
  TMD_setVERTEX(&v[25], -930, -480, -421);
  TMD_setVERTEX(&v[26], 828, 92, -105);
  TMD_setVERTEX(&v[27], 908, -288, -105);
  TMD_setVERTEX(&v[29], 1010, -524, 297);
  TMD_setVERTEX(&v[30], 1226, -176, 115);
  TMD_setVERTEX(&v[31], 934, 342, -357);
  TMD_setVERTEX(&v[32], 1190, 32, -335);
  TMD_setVERTEX(&v[33], 1322, 342, -627);
  TMD_setVERTEX(&v[34], 1870, 342, 141);
  TMD_setVERTEX(&v[35], -760, 212, -471);
  TMD_setVERTEX(&v[36], -1160, 212, -753);
  TMD_setVERTEX(&v[37], -1160, -182, -753);
  TMD_setVERTEX(&v[38], -1360, -494, -523);
  TMD_setVERTEX(&v[39], -1520, -966, -195);
  TMD_setVERTEX(&v[40], -1724, -394, -523);
  TMD_setVERTEX(&v[41], -1760, 94, -1037);
  TMD_setVERTEX(&v[42], -1656, -280, -1005);

  TMD_setCOLOUR(&rgb[0], 41, 106, 54);
  TMD_setCOLOUR(&rgb[1], 223, 230, 112);
  TMD_setCOLOUR(&rgb[2], 85, 186, 119);
  TMD_setCOLOUR(&rgb[3], 32, 186, 32);
  TMD_setCOLOUR(&rgb[4], 109, 220, 68);
  TMD_setCOLOUR(&rgb[5], 67, 187, 169);
  TMD_setCOLOUR(&rgb[6], 8, 214, 23);
  TMD_setCOLOUR(&rgb[7], 192, 246, 123);
  TMD_setCOLOUR(&rgb[8], 8, 112, 97);
  TMD_setCOLOUR(&rgb[9], 201, 217, 45);
/*  TMD_setCOLOUR(&rgb[10], 18, 114, 91);
  TMD_setCOLOUR(&rgb[11], 36, 126, 194);
  TMD_setCOLOUR(&rgb[12], 226, 218, 177);
  TMD_setCOLOUR(&rgb[13], 178, 4, 158);
  TMD_setCOLOUR(&rgb[14], 32, 128, 116);
  TMD_setCOLOUR(&rgb[15], 55, 154, 111);
  TMD_setCOLOUR(&rgb[16], 144, 12, 221);
  TMD_setCOLOUR(&rgb[17], 46, 94, 114);
  TMD_setCOLOUR(&rgb[18], 245, 9, 72);
  TMD_setCOLOUR(&rgb[19], 182, 88, 209);
  TMD_setCOLOUR(&rgb[20], 151, 233, 195);
  TMD_setCOLOUR(&rgb[21], 140, 177, 110);
  TMD_setCOLOUR(&rgb[22], 211, 221, 18);
  TMD_setCOLOUR(&rgb[23], 68, 98, 50);
  TMD_setCOLOUR(&rgb[24], 12, 20, 167);
  TMD_setCOLOUR(&rgb[25], 175, 63, 250);
  TMD_setCOLOUR(&rgb[26], 12, 222, 214);
  TMD_setCOLOUR(&rgb[27], 19, 206, 19);
  TMD_setCOLOUR(&rgb[29], 134, 203, 87);
  TMD_setCOLOUR(&rgb[30], 160, 71, 228);
  TMD_setCOLOUR(&rgb[31], 91, 190, 209);
  TMD_setCOLOUR(&rgb[32], 69, 180, 54);
  TMD_setCOLOUR(&rgb[33], 213, 136, 254);
  TMD_setCOLOUR(&rgb[34], 210, 0, 65);
  TMD_setCOLOUR(&rgb[35], 242, 135, 177);
  TMD_setCOLOUR(&rgb[36], 15, 131, 95);
  TMD_setCOLOUR(&rgb[37], 116, 101, 186);
  TMD_setCOLOUR(&rgb[38], 40, 70, 22);
  TMD_setCOLOUR(&rgb[39], 82, 223, 136);
  TMD_setCOLOUR(&rgb[40], 162, 19, 217);
  TMD_setCOLOUR(&rgb[41], 191, 66, 239);
  TMD_setCOLOUR(&rgb[42], 183, 17, 181);
*/
  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set3_NS_GP( 7, 21, 8, 7, 1, 8, false);
  TMD_set3_NS_GP( 8, 21, 14, 8, 1, 4, false);
  TMD_set3_NS_GP( 9, 8, 14, 9, 8, 4, false);
  TMD_set3_NS_GP( 9, 14, 15, 9, 4, 5, false);
  TMD_set3_NS_GP( 15, 14, 22, 5, 4, 2, false);
  TMD_set3_NS_GP( 15, 22, 16, 5, 2, 6, false);
  TMD_set3_NS_GP( 10, 15, 16, 0, 5, 6, false);
  TMD_set3_NS_GP( 10, 9, 15, 0, 9, 5, false);
  TMD_set3_NS_GP( 11, 10, 16, 1, 0, 6, false);
  TMD_set3_NS_GP( 11, 16, 17, 1, 6, 7, false);
  TMD_set3_NS_GP( 17, 16, 18, 7, 6, 8, false);
  TMD_set3_NS_GP( 17, 18, 20, 7, 8, 0, false);
  TMD_set3_NS_GP( 20, 19, 17, 0, 9, 7, false);
  TMD_set3_NS_GP( 11, 17, 12, 1, 7, 2, false);
  TMD_set3_NS_GP( 13, 12, 17, 3, 2, 7, false);
  TMD_set3_NS_GP( 13, 17, 19, 3, 7, 9, false);
  TMD_set3_NS_GP( 6, 13, 19, 6, 3, 9, false);
  TMD_set3_NS_GP( 6, 19, 27, 6, 9, 7, false);
  TMD_set3_NS_GP( 19, 20, 29, 9, 0, 9, false);
  TMD_set3_NS_GP( 29, 27, 19, 9, 7, 9, false);
  TMD_set3_NS_GP( 5, 6, 26, 5, 6, 6, false);
  TMD_set3_NS_GP( 4, 5, 26, 4, 5, 6, false);
  TMD_set3_NS_GP( 5, 31, 4, 5, 1, 4, false);
  TMD_set3_NS_GP( 31, 5, 26, 1, 5, 6, false);
  TMD_set3_NS_GP( 26, 6, 27, 6, 6, 7, false);
  TMD_set3_NS_GP( 34, 33, 32, 4, 3, 2, false);
  TMD_set3_NS_GP( 32, 31, 26, 2, 1, 6, false);
  TMD_set3_NS_GP( 32, 33, 31, 2, 3, 1, false);
  TMD_set3_NS_GP( 32, 26, 27, 2, 6, 7, false);
  TMD_set3_NS_GP( 32, 27, 30, 2, 7, 0, false);
  TMD_set3_NS_GP( 30, 27, 29, 0, 7, 9, false);
  TMD_set3_NS_GP( 34, 32, 30, 4, 2, 0, false);
  TMD_set3_NS_GP( 18, 16, 22, 8, 6, 2, false);
  TMD_set3_NS_GP( 24, 23, 25, 4, 3, 5, false);
  TMD_set3_NS_GP( 24, 21, 23, 4, 1, 3, false);
  TMD_set3_NS_GP( 21, 7, 23, 1, 7, 3, false);
  TMD_set3_NS_GP( 7, 0, 23, 7, 0, 3, false);
  TMD_set3_NS_GP( 23, 0, 35, 3, 0, 5, false);
  TMD_set3_NS_GP( 0, 1, 35, 0, 1, 5, false);
  TMD_set3_NS_GP( 1, 2, 35, 1, 2, 5, false);
  TMD_set3_NS_GP( 25, 23, 35, 5, 3, 5, false);
  TMD_set3_NS_GP( 39, 25, 38, 9, 5, 8, false);
  TMD_set3_NS_GP( 39, 24, 25, 9, 4, 5, false);
  TMD_set3_NS_GP( 38, 25, 37, 8, 5, 7, false);
  TMD_set3_NS_GP( 37, 25, 35, 7, 5, 5, false);
  TMD_set3_NS_GP( 35, 36, 37, 5, 6, 7, false);
  TMD_set3_NS_GP( 38, 40, 39, 8, 0, 9, false);
  TMD_set3_NS_GP( 1, 5, 2, 1, 5, 2, false);
  TMD_set3_NS_GP( 2, 5, 4, 2, 5, 4, false);
  TMD_set3_NS_GP( 42, 40, 37, 2, 0, 7, false);
  TMD_set3_NS_GP( 37, 40, 38, 7, 0, 8, false);
//  TMD_set3_NS_GP( 42, 37, 41, 2, 7, 1, false);
//  TMD_set3_NS_GP( 41, 37, 36, 1, 7, 6, false);

  TMD_START_OBJ[OBJ_TUNNEL_ENTRANCE] = TMD_finish();
  }
