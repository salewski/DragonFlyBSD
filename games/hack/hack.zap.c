/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.zap.c - version 1.0.3 */
/* $FreeBSD: src/games/hack/hack.zap.c,v 1.4 1999/11/16 10:26:38 marcel Exp $ */
/* $DragonFly: src/games/hack/hack.zap.c,v 1.3 2004/11/06 12:29:17 eirikn Exp $ */

#include "hack.h"

extern struct obj *mkobj_at();
extern struct monst *makemon(), *mkmon_at(), youmonst;
struct monst *bhit();
char *exclam();

char *fl[]= {
	"magic missile",
	"bolt of fire",
	"sleep ray",
	"bolt of cold",
	"death ray"
};

/* Routines for IMMEDIATE wands. */
/* bhitm: monster mtmp was hit by the effect of wand otmp */
bhitm(mtmp, otmp)
struct monst *mtmp;
struct obj *otmp;
{
	wakeup(mtmp);
	switch(otmp->otyp) {
	case WAN_STRIKING:
		if(u.uswallow || rnd(20) < 10+mtmp->data->ac) {
			int tmp = d(2,12);
			hit("wand", mtmp, exclam(tmp));
			mtmp->mhp -= tmp;
			if(mtmp->mhp < 1) killed(mtmp);
		} else miss("wand", mtmp);
		break;
	case WAN_SLOW_MONSTER:
		mtmp->mspeed = MSLOW;
		break;
	case WAN_SPEED_MONSTER:
		mtmp->mspeed = MFAST;
		break;
	case WAN_UNDEAD_TURNING:
		if(index(UNDEAD,mtmp->data->mlet)) {
			mtmp->mhp -= rnd(8);
			if(mtmp->mhp < 1) killed(mtmp);
			else mtmp->mflee = 1;
		}
		break;
	case WAN_POLYMORPH:
		if( newcham(mtmp,&mons[rn2(CMNUM)]) )
			objects[otmp->otyp].oc_name_known = 1;
		break;
	case WAN_CANCELLATION:
		mtmp->mcan = 1;
		break;
	case WAN_TELEPORTATION:
		rloc(mtmp);
		break;
	case WAN_MAKE_INVISIBLE:
		mtmp->minvis = 1;
		break;
#ifdef WAN_PROBING
	case WAN_PROBING:
		mstatusline(mtmp);
		break;
#endif /* WAN_PROBING */
	default:
		impossible("What an interesting wand (%u)", otmp->otyp);
	}
}

bhito(obj, otmp)	/* object obj was hit by the effect of wand otmp */
struct obj *obj, *otmp;	/* returns TRUE if sth was done */
{
	int res = TRUE;

	if(obj == uball || obj == uchain)
		res = FALSE;
	else
	switch(otmp->otyp) {
	case WAN_POLYMORPH:
		/* preserve symbol and quantity, but turn rocks into gems */
		mkobj_at((obj->otyp == ROCK || obj->otyp == ENORMOUS_ROCK)
			? GEM_SYM : obj->olet,
			obj->ox, obj->oy) -> quan = obj->quan;
		delobj(obj);
		break;
	case WAN_STRIKING:
		if(obj->otyp == ENORMOUS_ROCK)
			fracture_rock(obj);
		else
			res = FALSE;
		break;
	case WAN_CANCELLATION:
		if(obj->spe && obj->olet != AMULET_SYM) {
			obj->known = 0;
			obj->spe = 0;
		}
		break;
	case WAN_TELEPORTATION:
		rloco(obj);
		break;
	case WAN_MAKE_INVISIBLE:
		obj->oinvis = 1;
		break;
	case WAN_UNDEAD_TURNING:
		res = revive(obj);
		break;
	case WAN_SLOW_MONSTER:		/* no effect on objects */
	case WAN_SPEED_MONSTER:
#ifdef WAN_PROBING
	case WAN_PROBING:
#endif /* WAN_PROBING */
		res = FALSE;
		break;
	default:
		impossible("What an interesting wand (%u)", otmp->otyp);
	}
	return(res);
}

dozap()
{
	struct obj *obj;
	xchar zx,zy;

	obj = getobj("/", "zap");
	if(!obj) return(0);
	if(obj->spe < 0 || (obj->spe == 0 && rn2(121))) {
		pline("Nothing Happens.");
		return(1);
	}
	if(obj->spe == 0)
		pline("You wrest one more spell from the worn-out wand.");
	if(!(objects[obj->otyp].bits & NODIR) && !getdir(1))
		return(1);	/* make him pay for knowing !NODIR */
	obj->spe--;
	if(objects[obj->otyp].bits & IMMEDIATE) {
		if(u.uswallow)
			bhitm(u.ustuck, obj);
		else if(u.dz) {
			if(u.dz > 0) {
				struct obj *otmp = o_at(u.ux, u.uy);
				if(otmp)
					(void) bhito(otmp, obj);
			}
		} else
			(void) bhit(u.dx,u.dy,rn1(8,6),0,bhitm,bhito,obj);
	} else {
	    switch(obj->otyp){
		case WAN_LIGHT:
			litroom(TRUE);
			break;
		case WAN_SECRET_DOOR_DETECTION:
			if(!findit()) return(1);
			break;
		case WAN_CREATE_MONSTER:
			{ int cnt = 1;
			if(!rn2(23)) cnt += rn2(7) + 1;
			while(cnt--)
			    (void) makemon((struct permonst *) 0, u.ux, u.uy);
			}
			break;
		case WAN_WISHING:
			{ char buf[BUFSZ];
			  struct obj *otmp;
			  extern struct obj *readobjnam(), *addinv();
		      if(u.uluck + rn2(5) < 0) {
			pline("Unfortunately, nothing happens.");
			break;
		      }
		      pline("You may wish for an object. What do you want? ");
		      getlin(buf);
		      if(buf[0] == '\033') buf[0] = 0;
		      otmp = readobjnam(buf);
		      otmp = addinv(otmp);
		      prinv(otmp);
		      break;
			}
		case WAN_DIGGING:
			/* Original effect (approximately):
			 * from CORR: dig until we pierce a wall
			 * from ROOM: piece wall and dig until we reach
			 * an ACCESSIBLE place.
			 * Currently: dig for digdepth positions;
			 * also down on request of Lennart Augustsson.
			 */
			{ struct rm *room;
			  int digdepth;
			if(u.uswallow) {
				struct monst *mtmp = u.ustuck;

				pline("You pierce %s's stomach wall!",
					monnam(mtmp));
				mtmp->mhp = 1;	/* almost dead */
				unstuck(mtmp);
				mnexto(mtmp);
				break;
			}
			if(u.dz) {
			    if(u.dz < 0) {
				pline("You loosen a rock from the ceiling.");
				pline("It falls on your head!");
				losehp(1, "falling rock");
				mksobj_at(ROCK, u.ux, u.uy);
				fobj->quan = 1;
				stackobj(fobj);
				if(Invisible) newsym(u.ux, u.uy);
			    } else {
				dighole();
			    }
			    break;
			}
			zx = u.ux+u.dx;
			zy = u.uy+u.dy;
			digdepth = 8 + rn2(18);
			Tmp_at(-1, '*');	/* open call */
			while(--digdepth >= 0) {
				if(!isok(zx,zy)) break;
				room = &levl[zx][zy];
				Tmp_at(zx,zy);
				if(!xdnstair){
					if(zx < 3 || zx > COLNO-3 ||
					    zy < 3 || zy > ROWNO-3)
						break;
					if(room->typ == HWALL ||
					    room->typ == VWALL){
						room->typ = ROOM;
						break;
					}
				} else
				if(room->typ == HWALL || room->typ == VWALL ||
				   room->typ == SDOOR || room->typ == LDOOR){
					room->typ = DOOR;
					digdepth -= 2;
				} else
				if(room->typ == SCORR || !room->typ) {
					room->typ = CORR;
					digdepth--;
				}
				mnewsym(zx,zy);
				zx += u.dx;
				zy += u.dy;
			}
			mnewsym(zx,zy);	/* not always necessary */
			Tmp_at(-1,-1);	/* closing call */
			break;
			}
		default:
			buzz((int) obj->otyp - WAN_MAGIC_MISSILE,
				u.ux, u.uy, u.dx, u.dy);
			break;
		}
		if(!objects[obj->otyp].oc_name_known) {
			objects[obj->otyp].oc_name_known = 1;
			more_experienced(0,10);
		}
	}
	return(1);
}

char *
exclam(force)
int force;
{
	/* force == 0 occurs e.g. with sleep ray */
	/* note that large force is usual with wands so that !! would
		require information about hand/weapon/wand */
	return( (force < 0) ? "?" : (force <= 4) ? "." : "!" );
}

hit(str,mtmp,force)
char *str;
struct monst *mtmp;
char *force;		/* usually either "." or "!" */
{
	if(!cansee(mtmp->mx,mtmp->my)) pline("The %s hits it.", str);
	else pline("The %s hits %s%s", str, monnam(mtmp), force);
}

miss(str,mtmp)
char *str;
struct monst *mtmp;
{
	if(!cansee(mtmp->mx,mtmp->my)) pline("The %s misses it.",str);
	else pline("The %s misses %s.",str,monnam(mtmp));
}

/* bhit: called when a weapon is thrown (sym = obj->olet) or when an
   IMMEDIATE wand is zapped (sym = 0); the weapon falls down at end of
   range or when a monster is hit; the monster is returned, and bhitpos
   is set to the final position of the weapon thrown; the ray of a wand
   may affect several objects and monsters on its path - for each of
   these an argument function is called. */
/* check !u.uswallow before calling bhit() */

struct monst *
bhit(ddx,ddy,range,sym,fhitm,fhito,obj)
int ddx,ddy,range;		/* direction and range */
char sym;				/* symbol displayed on path */
int (*fhitm)(), (*fhito)();		/* fns called when mon/obj hit */
struct obj *obj;			/* 2nd arg to fhitm/fhito */
{
	struct monst *mtmp;
	struct obj *otmp;
	int typ;

	bhitpos.x = u.ux;
	bhitpos.y = u.uy;

	if(sym) tmp_at(-1, sym);	/* open call */
	while(range-- > 0) {
		bhitpos.x += ddx;
		bhitpos.y += ddy;
		typ = levl[bhitpos.x][bhitpos.y].typ;
		if(mtmp = m_at(bhitpos.x,bhitpos.y)){
			if(sym) {
				tmp_at(-1, -1);	/* close call */
				return(mtmp);
			}
			(*fhitm)(mtmp, obj);
			range -= 3;
		}
		if(fhito && (otmp = o_at(bhitpos.x,bhitpos.y))){
			if((*fhito)(otmp, obj))
				range--;
		}
		if(!ZAP_POS(typ)) {
			bhitpos.x -= ddx;
			bhitpos.y -= ddy;
			break;
		}
		if(sym) tmp_at(bhitpos.x, bhitpos.y);
	}

	/* leave last symbol unless in a pool */
	if(sym)
	   tmp_at(-1, (levl[bhitpos.x][bhitpos.y].typ == POOL) ? -1 : 0);
	return(0);
}

struct monst *
boomhit(dx,dy) {
	int i, ct;
	struct monst *mtmp;
	char sym = ')';
	extern schar xdir[], ydir[];

	bhitpos.x = u.ux;
	bhitpos.y = u.uy;

	for(i=0; i<8; i++) if(xdir[i] == dx && ydir[i] == dy) break;
	tmp_at(-1, sym);	/* open call */
	for(ct=0; ct<10; ct++) {
		if(i == 8) i = 0;
		sym = ')' + '(' - sym;
		tmp_at(-2, sym);	/* change let call */
		dx = xdir[i];
		dy = ydir[i];
		bhitpos.x += dx;
		bhitpos.y += dy;
		if(mtmp = m_at(bhitpos.x, bhitpos.y)){
			tmp_at(-1,-1);
			return(mtmp);
		}
		if(!ZAP_POS(levl[bhitpos.x][bhitpos.y].typ)) {
			bhitpos.x -= dx;
			bhitpos.y -= dy;
			break;
		}
		if(bhitpos.x == u.ux && bhitpos.y == u.uy) { /* ct == 9 */
			if(rn2(20) >= 10+u.ulevel){	/* we hit ourselves */
				(void) thitu(10, rnd(10), "boomerang");
				break;
			} else {	/* we catch it */
				tmp_at(-1,-1);
				pline("Skillfully, you catch the boomerang.");
				return(&youmonst);
			}
		}
		tmp_at(bhitpos.x, bhitpos.y);
		if(ct % 5 != 0) i++;
	}
	tmp_at(-1, -1);	/* do not leave last symbol */
	return(0);
}

char
dirlet(dx,dy) int dx,dy; {
	return
		(dx == dy) ? '\\' : (dx && dy) ? '/' : dx ? '-' : '|';
}

/* type == -1: monster spitting fire at you */
/* type == -1,-2,-3: bolts sent out by wizard */
/* called with dx = dy = 0 with vertical bolts */
buzz(type,sx,sy,dx,dy)
int type;
xchar sx,sy;
int dx,dy;
{
	int abstype = abs(type);
	char *fltxt = (type == -1) ? "blaze of fire" : fl[abstype];
	struct rm *lev;
	xchar range;
	struct monst *mon;

	if(u.uswallow) {
		int tmp;

		if(type < 0) return;
		tmp = zhit(u.ustuck, type);
		pline("The %s rips into %s%s",
			fltxt, monnam(u.ustuck), exclam(tmp));
		return;
	}
	if(type < 0) pru();
	range = rn1(7,7);
	Tmp_at(-1, dirlet(dx,dy));	/* open call */
	while(range-- > 0) {
		sx += dx;
		sy += dy;
		if((lev = &levl[sx][sy])->typ) Tmp_at(sx,sy);
		else {
			int bounce = 0;
			if(cansee(sx-dx,sy-dy))
				pline("The %s bounces!", fltxt);
			if(ZAP_POS(levl[sx][sy-dy].typ))
				bounce = 1;
			if(ZAP_POS(levl[sx-dx][sy].typ)) {
				if(!bounce || rn2(2)) bounce = 2;
			}
			switch(bounce){
			case 0:
				dx = -dx;
				dy = -dy;
				continue;
			case 1:
				dy = -dy;
				sx -= dx;
				break;
			case 2:
				dx = -dx;
				sy -= dy;
				break;
			}
			Tmp_at(-2,dirlet(dx,dy));
			continue;
		}
		if(lev->typ == POOL && abstype == 1 /* fire */) {
			range -= 3;
			lev->typ = ROOM;
			if(cansee(sx,sy)) {
				mnewsym(sx,sy);
				pline("The water evaporates.");
			} else
				pline("You hear a hissing sound.");
		}
		if((mon = m_at(sx,sy)) &&
		   (type != -1 || mon->data->mlet != 'D')) {
			wakeup(mon);
			if(rnd(20) < 18 + mon->data->ac) {
				int tmp = zhit(mon,abstype);
				if(mon->mhp < 1) {
					if(type < 0) {
					    if(cansee(mon->mx,mon->my))
					      pline("%s is killed by the %s!",
						Monnam(mon), fltxt);
					    mondied(mon);
					} else
					    killed(mon);
				} else
					hit(fltxt, mon, exclam(tmp));
				range -= 2;
			} else
				miss(fltxt,mon);
		} else if(sx == u.ux && sy == u.uy) {
			nomul(0);
			if(rnd(20) < 18+u.uac) {
				int dam = 0;
				range -= 2;
				pline("The %s hits you!",fltxt);
				switch(abstype) {
				case 0:
					dam = d(2,6);
					break;
				case 1:
					if(Fire_resistance)
						pline("You don't feel hot!");
					else dam = d(6,6);
					if(!rn2(3))
						burn_scrolls();
					break;
				case 2:
					nomul(-rnd(25)); /* sleep ray */
					break;
				case 3:
					if(Cold_resistance)
						pline("You don't feel cold!");
					else dam = d(6,6);
					break;
				case 4:
					u.uhp = -1;
				}
				losehp(dam,fltxt);
			} else pline("The %s whizzes by you!",fltxt);
			stop_occupation();
		}
		if(!ZAP_POS(lev->typ)) {
			int bounce = 0, rmn;
			if(cansee(sx,sy)) pline("The %s bounces!",fltxt);
			range--;
			if(!dx || !dy || !rn2(20)){
				dx = -dx;
				dy = -dy;
			} else {
			  if(ZAP_POS(rmn = levl[sx][sy-dy].typ) &&
			    (IS_ROOM(rmn) || ZAP_POS(levl[sx+dx][sy-dy].typ)))
				bounce = 1;
			  if(ZAP_POS(rmn = levl[sx-dx][sy].typ) &&
			    (IS_ROOM(rmn) || ZAP_POS(levl[sx-dx][sy+dy].typ)))
				if(!bounce || rn2(2))
					bounce = 2;

			  switch(bounce){
			  case 0:
				dy = -dy;
				dx = -dx;
				break;
			  case 1:
				dy = -dy;
				break;
			  case 2:
				dx = -dx;
				break;
			  }
			  Tmp_at(-2, dirlet(dx,dy));
			}
		}
	}
	Tmp_at(-1,-1);
}

zhit(mon,type)			/* returns damage to mon */
struct monst *mon;
int type;
{
	int tmp = 0;

	switch(type) {
	case 0:			/* magic missile */
		tmp = d(2,6);
		break;
	case -1:		/* Dragon blazing fire */
	case 1:			/* fire */
		if(index("Dg", mon->data->mlet)) break;
		tmp = d(6,6);
		if(index("YF", mon->data->mlet)) tmp += 7;
		break;
	case 2:			/* sleep*/
		mon->mfroz = 1;
		break;
	case 3:			/* cold */
		if(index("YFgf", mon->data->mlet)) break;
		tmp = d(6,6);
		if(mon->data->mlet == 'D') tmp += 7;
		break;
	case 4:			/* death*/
		if(index(UNDEAD, mon->data->mlet)) break;
		tmp = mon->mhp+1;
		break;
	}
	mon->mhp -= tmp;
	return(tmp);
}

#define	CORPSE_I_TO_C(otyp)	(char) ((otyp >= DEAD_ACID_BLOB)\
		     ?  'a' + (otyp - DEAD_ACID_BLOB)\
		     :	'@' + (otyp - DEAD_HUMAN))
revive(obj)
struct obj *obj;
{
	struct monst *mtmp;

	if(obj->olet == FOOD_SYM && obj->otyp > CORPSE) {
		/* do not (yet) revive shopkeepers */
		/* Note: this might conceivably produce two monsters
			at the same position - strange, but harmless */
		mtmp = mkmon_at(CORPSE_I_TO_C(obj->otyp),obj->ox,obj->oy);
		delobj(obj);
	}
	return(!!mtmp);		/* TRUE if some monster created */
}

rloco(obj)
struct obj *obj;
{
	int tx,ty,otx,oty;

	otx = obj->ox;
	oty = obj->oy;
	do {
		tx = rn1(COLNO-3,2);
		ty = rn2(ROWNO);
	} while(!goodpos(tx,ty));
	obj->ox = tx;
	obj->oy = ty;
	if(cansee(otx,oty))
		newsym(otx,oty);
}

fracture_rock(obj)	/* fractured by pick-axe or wand of striking */
struct obj *obj;			   /* no texts here! */
{
	/* unpobj(obj); */
	obj->otyp = ROCK;
	obj->quan = 7 + rn2(60);
	obj->owt = weight(obj);
	obj->olet = WEAPON_SYM;
	if(cansee(obj->ox,obj->oy))
		prl(obj->ox,obj->oy);
}

burn_scrolls()
{
	struct obj *obj, *obj2;
	int cnt = 0;

	for(obj = invent; obj; obj = obj2) {
		obj2 = obj->nobj;
		if(obj->olet == SCROLL_SYM) {
			cnt++;
			useup(obj);
		}
	}
	if(cnt > 1) {
		pline("Your scrolls catch fire!");
		losehp(cnt, "burning scrolls");
	} else if(cnt) {
		pline("Your scroll catches fire!");
		losehp(1, "burning scroll");
	}
}
