/* pr_process.c
 * by Dennis Chao
 * modified by David Koppenhofer
 * modified again by Gideon Redelinghuys
 */
// Copyright (C) 1999 by Dennis Chao
// Copyright (C) 2000 by David Koppenhofer
// Copyright (C) 2015 by Gideon Redelinghuys

#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "doomstat.h"
#include "info.h"
#include "doomdef.h"
#include "pr_process.h"

#if defined(SCOOS5) || defined(SCOUW2) || defined(SCOUW7)
#include "strcmp.h"
#endif

// Have this routine return a pointer to the pid mobj it creates.
// Return NULL if nothing is created.
// Also accept a parameter to tell if it is spawning a pid mobj.
mobj_t *P_SpawnMapThing(mapthing_t*	mthing, boolean is_pid_mobj); /* from p_mobj.c */

// Use this to see if the newly created pid mobj is colliding
// with anything.
// True if OK, False if collision.
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y); /* from p_map.c */

// Use this to remove the mobj without killing it (thus leaving a body).
// Pass S_NULL as the state to do this.
boolean P_SetMobjState(mobj_t *mobj, statenum_t state); /* from p_mobj.c */

// Use this to kill off the mobj when it is still in Doom, but the
// corresponding process is not running.
void P_KillMobj(mobj_t *source, mobj_t *target); /* from p_inter.c */

// This routine does quite a bit of stuff.  See the description later
// in this file.
void add_to_pid_list(int pid, const char *name, int demon);

// Need to return the newly created mobj_t from this routine now.
mobj_t *add_new_process(int pid, const char *pname, int demon);

// This routine is called for custom level processing only.
// It guarantees the mt_ext.x and mt_ext.y are at least
// the monster's radius inside its bounding spawn box.
// The last argument is the lowest x coordinate for that
// particular process' area.
void clip_to_spawnbox(short *x, short *y, int radius, int box_min_x);

// Structure to hold linked list of usernames.
typedef struct ps_userlist_s
{
        char                    name[256];
        struct ps_userlist_s*   next;

} ps_userlist_t;

// Head of the list of users to include.
static ps_userlist_t *psuser_list_head = NULL;

// Head of the list of users to exclude.
static ps_userlist_t *psnotuser_list_head = NULL;

// Need some pointers to the linked list of pid mobjs.
// (head, tail, and current position)
// Probably could have done a circular linked list ... oh well. :-)
static mobj_t *pid_list_head = NULL;
static mobj_t *pid_list_tail = NULL;
static mobj_t *pid_list_pos = NULL;

static int global_pid_counter = 0;

int hash(unsigned char *str)
{
    int hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    if (hash < 0) {
        hash = 0 - hash;
    }

    return hash;
}

// pr_check
// This routine replaces both pr_init() and pr_poll().  I used some code
// from those routines to make this one, though.
// Called at the start of a game/level and periodically throughout the
// playing of the level.  It runs 'ps', capturing certain info about each
// process except for 'ps' itself.
void pr_check(void) {

// Don't open 'ps' right away; see if we're on the right level first,
// amongst other criteria.
  FILE *f = NULL;

  char buf[256];
  int pid = 0;
  char namebuf[256];
  char tty[256];
  int demon = false;

// Bypass the 'ps' part of the program entirely if it's an add-on pack
// to Doom 2 OR we're not on the correct level (e1m1 or map01).
// Also bypass if we're recording or viewing a demo.
// Also bypass if the -nopsmon flag was given on the command line.
  if ( (gamemap != 1) ||
       (gameepisode > 1) ||
       (gamemission != doom && gamemission != doom2) ||
       (demorecording) ||
       (demoplayback) ||
       (gamestate == GS_DEMOSCREEN) ||
       (nopsmon)
     )
  {
     return;
  }

  f = popen("echo list | nc -U /dockerdoom.socket", "r");

  if (!f) {
    fprintf(stderr, "ERROR: pr_check could not open ps\n");
    return;
  }

  /* Read in all process information.  Exclude the last process in the
     list. */

  while (fgets(buf, 255, f)) {
    int read_fields = sscanf(buf, "%s\n", namebuf);
    if (read_fields == 1 && namebuf) {
      pid = hash(namebuf);
      fprintf(stderr, "Demon: %s, %d\n", namebuf, pid);
      demon = true;
      add_to_pid_list(pid, namebuf, demon);
    }
  }

  pclose(f);
}

// add_to_pid_list
//   The routine add_to_pid_list() does a lot.  It walks the
// pid mobj linked list to check an actual process versus the
// Doom monster status.  It is sorted in ascending pid order.
//   If this process is not in the list, a monster is spawned
// before linking it into the list.  Unset the delete flag to
// ensure the process is not deleted from the list during cleanup.
//   If this process is already in the list, unset the flag that says
// "delete me from the list during next list cleanup" and update
// the pid monster's name.  If the process is in the list and Doom
// killed it since the last list cleanup, resurrect the monster like
// the archvile does (because the process didn't really die).
// If we can't resurrect it in that case, remove the body and spawn
// a new monster.
void add_to_pid_list(int pid, const char *name, int demon) {

   mobj_t	*new_mobj = NULL;
   int		name_length = 0;
   boolean	position_ok = false;

// If the list isn't empty and the new pid is larger than the largest
// pid currently in the list, spawn the mobj and link it as the tail
// of the list if we placed it on the map without a collision.
   if ( pid_list_tail && pid > pid_list_tail->m_pid ) {
      if ( (new_mobj = add_new_process(pid, name, demon)) != NULL ) {
         new_mobj->ppid = pid_list_tail;
         new_mobj->npid = NULL;
         pid_list_tail->npid = new_mobj;
         pid_list_tail = new_mobj;
         new_mobj->m_del_from_pid_list = false;
      }
// If the list isn't empty and the new pid is smaller than the smallest
// pid currently in the list, spawn the mobj and link it as the head
// of the list if we placed it on the map without a collision.
   } else if ( pid_list_head && pid < pid_list_head->m_pid ) {
      if ( (new_mobj = add_new_process(pid, name, demon)) != NULL ) {
         new_mobj->ppid = NULL;
         new_mobj->npid = pid_list_head;
         pid_list_head->ppid = new_mobj;
         pid_list_head = new_mobj;
         new_mobj->m_del_from_pid_list = false;
      }
// If the read-in pid is not larger or smaller than the extremes of
// the list, walk down the list until the current pointer is at the
// mobj with the pid ahead of the read-in pid.
   } else {
      pid_list_pos = pid_list_head;
      while ( pid_list_pos != NULL && pid > pid_list_pos->m_pid ) {

// NOTE:
// If we were guaranteed that the pid's were coming in ascending order
// from ps, we could do the deletion of unused nodes here and the code
// would be much more efficient.  However, we sort ps output by the
// process start time.  This could lead to smaller pid's coming first
// when the pid counter wraps from the max back to smaller numbers.
         pid_list_pos = pid_list_pos->npid;

      }  // end while

// Now, check why we stopped the walk down the list.
// First, check to see if this is an empty list.  If it isn't empty,
// check whether the mobj's pid is equal or if it's larger than the
// read-in pid.
// If the list is completely empty, spawn the mobj and make it the
// only member of the list.
      if ( pid_list_pos != NULL ) {  // if list isn't empty
         if ( pid == pid_list_pos->m_pid ) {
            // if the read-in pid is same as mobj's pid, set the
            // delete flag to false so the mobj is not unlinked
            // from the list during cleanup.
            pid_list_pos->m_del_from_pid_list = false;

            // copy the incoming pid's name over the mobj's name
            // in case an exec() was called by the process.
            // ie. mingetty --> bash
// TODO: Make name change conditional?  Or is memcpy() fast enough
//       that we can do it every time?
            name_length = strlen( name );
            if ( name_length <= 31 ) {
               memcpy(pid_list_pos->m_pname, name, 32);
            }
            else {
               memcpy(pid_list_pos->m_pname, name + name_length - 31, 32);
            }

            // if the process is still running on the machine, but
            // the mobj has been killed in Doom (ie. the process
            // trapped the kill signal or the user didn't have permission
            // to kill the process), do an archvile-like resurrection
            // on the mobj.  if we can't do the resurrection, remove the
            // body and re-spawn the mobj normally.

            // much of the following code is taken from the routines
            // PIT_VileCheck() and A_VileChase() in p_enemy.c

            if ( !(pid_list_pos->m_draw_pid_info) ) {  // if Doom thinks
                                                       // the process is
                                                       // dead

               // ability to resurrect monster (from PIT_VileCheck).
               pid_list_pos->momx = pid_list_pos->momy = 0;
               pid_list_pos->height <<= 2;
               position_ok = P_CheckPosition(pid_list_pos,
                                             pid_list_pos->x,
                                             pid_list_pos->y);
               pid_list_pos->height >>= 2;

               // if any of these conditions, we CAN'T resurrect
               if ( !(pid_list_pos->flags & MF_CORPSE)  || 
                     (pid_list_pos->tics != -1)  ||
                     (pid_list_pos->info->raisestate == S_NULL) ||
                    !(position_ok)  ) {  // can't raise it: delete and respawn

                  // print status msg
                  fprintf(stderr, "   process %d [%s] monster delete/respawn\n",
                                 pid_list_pos->m_pid,
                                 pid_list_pos->m_pname);

                  // In this case, we need to respawn the monster
                  // since we can't resurrect it.  We delete the mobj
                  // from the pid linked list so it will re-spawn on
                  // the next call to add_to_pid_list().
                  pid_list_pos->m_del_from_pid_list = true;

                  // trick cleanup_pid_list() into thinking it needs
                  // to remove this body
                  pid_list_pos->m_draw_pid_info = true;

                  // remove this mobj from pid list (and remove body)
                  cleanup_pid_list(pid_list_pos);

                  // try to respawn monster
                  add_to_pid_list(pid, name, demon);

               } else {  // we can raise the monster's body

                  // print status msg
                  fprintf(stderr, "   process %d [%s] monster resurrect\n",
                                 pid_list_pos->m_pid,
                                 pid_list_pos->m_pname);

                  // most of this is from A_VileChase()
                  P_SetMobjState(pid_list_pos, pid_list_pos->info->raisestate);
                  pid_list_pos->height <<= 2;
                  pid_list_pos->flags = pid_list_pos->info->flags;
                  pid_list_pos->health = pid_list_pos->info->spawnhealth;
                  pid_list_pos->target = NULL;

                  // draw the pid info again
                  pid_list_pos->m_draw_pid_info = true;

                  // need to unset the flag (again) to not count this
                  // monster in end-of-level kill % -- it was reset when
                  // the mobj flags were copied from the info record above
                  pid_list_pos->flags &= ~MF_COUNTKILL;

               }

            }  // end 'if the monster's dead but not the process.'

         } else {  // the read-in pid is smaller than the currently
                   // pointed to mobj.  spawn a new mobj and link it
                   // into the list if it is placed without a collision.
            if ( (new_mobj = add_new_process(pid, name, demon)) != NULL ) {
               new_mobj->ppid = pid_list_pos->ppid;
               new_mobj->npid = pid_list_pos;
               pid_list_pos->ppid->npid = new_mobj;
               pid_list_pos->ppid = new_mobj;
               new_mobj->m_del_from_pid_list = false;
            }
         }
      } else {  // empty list.  spawn the mobj and make it the only
                // member of the list if it is placed without a collision.
         if ( (new_mobj = add_new_process(pid, name, demon)) != NULL ) {
            pid_list_head = pid_list_tail = new_mobj;
            new_mobj->npid = new_mobj->ppid = NULL;
            new_mobj->m_del_from_pid_list = false;
         }
      }
   }

}

// cleanup_pid_list
// This routine deletes all processes from the linked list and (maybe)
// Doom that weren't in the last run of 'ps'.  It also sets the delete
// flag for all remaining processes in the pid linked list.
// If a mobj pointer is passed to this routine, it tries to cleanup
// that mobj only.  If NULL is passed in, it does cleanup on all
// mobj's.
// Note: If this routine is called with a NULL argument twice in a
// row without calling add_to_pid_list() in between, all pid monsters
// are deleted.
void cleanup_pid_list(mobj_t *del_mobj) {

   mobj_t	*pid_list_pos_npid_hold = NULL;

   if ( del_mobj != NULL ) {
      // jump directly to the passed-in mobj
      pid_list_pos = del_mobj;
   } else {
      // start at the head of the list
      pid_list_pos = pid_list_head;
   }

// walk down the list until the end
   while ( pid_list_pos != NULL ) {

      // assign the next mobj to a temporary pointer in case
      // we have to mess with the current mobj pointer
      if ( del_mobj != NULL ) {
         // make the next pointer NULL, so the while loop stops
         pid_list_pos_npid_hold = NULL;
      } else {
         // walk down the list
         pid_list_pos_npid_hold = pid_list_pos->npid;
      }

      // if the process is flagged for deletion, delete it
      // from the pid linked list.
      if ( pid_list_pos->m_del_from_pid_list ) {

         if ( pid_list_pos == pid_list_head ) {  // remove head
            pid_list_head = pid_list_pos->npid;
            if ( pid_list_pos->npid != NULL ){
               pid_list_pos->npid->ppid = NULL;
               pid_list_pos->npid = NULL;
            }
         } else if (pid_list_pos == pid_list_tail ) {  // remove tail
            pid_list_tail = pid_list_pos->ppid;
            if( pid_list_pos->ppid != NULL ) {
               pid_list_pos->ppid->npid = NULL;
               pid_list_pos->ppid = NULL;
            }
         } else {  // remove central node
            pid_list_pos->ppid->npid = pid_list_pos->npid;
            pid_list_pos->npid->ppid = pid_list_pos->ppid;
            pid_list_pos->ppid = pid_list_pos->npid = NULL;
         }

         if ( pid_list_pos->m_draw_pid_info ) {
            // if the pid info is still being drawn, Doom doesn't know
            // that the process is gone (not killed in Doom).  remove it,
            // without leaving a body or dropped weapon.
// NOTE:
// If only the P_SetMobjState() is used below, attackers of this process
// will continue to attack the empty space.  So we have to kludge
// around by setting the delete flag and then calling the kill routine.
// Then, the only time the delete flag is true in P_KillMobj() is if
// P_KillMobj() is called from this routine.
// In P_KillMobj(), we check the delete flag and m_pid before sending
// a system kill command and again before spawning a dropped object.
            // print status msg
            fprintf(stderr, "   process %d [%s] monster removed from engine\n",
                                 pid_list_pos->m_pid,
                                 pid_list_pos->m_pname);

            // set flag to check for in the kill routine
            pid_list_pos->m_del_from_pid_list = false;

            // kill the monster, don't drop an item
            P_KillMobj(NULL, pid_list_pos);

            // remove the body
            P_SetMobjState(pid_list_pos, S_NULL);
         }
      } else {
         // if the process was in the last ps run, set it's delete flag
         // so it will be cleaned up next time (unless it vouches
         // for itself again next time ps is run).
         pid_list_pos->m_del_from_pid_list = true;
      }
      // walk to the next node on the pid mobj list.
      pid_list_pos = pid_list_pos_npid_hold;

   }  // end while node walk

}


// add_new_process
// Need to return the newly created mobj_t from this routine, or
// NULL if the monster couldn't be spawned due to collisions.
//
mobj_t *add_new_process(int pid, const char *pname, int demon) {

    mapthing_t	mt_ext;

// Add a variable for pname's length
    int			pname_length = 0;

// Variables to hold original x,y of mt_ext.
    int			orig_x = 0;
    int			orig_y = 0;

// Holds the radius of the monster in map units.
    int			monster_radius = 0;

// Variable to hold the pid mobj we just created.  Needed for collision
// check function and possible removal of the mobj.
    mobj_t		*pid_mobj;

// Variables to control monster placement.  Use 3 nested for loops to
// alter location by the monster radius.  A flag is used to break out
// of the nested loop when we successfully place the monster.
    int			i = 0;
    int			j = 0;
    int			k = 0;
    boolean		monster_placed = false;

// This holds the bounding box number for placement calculations.
// Only assigned >= 0 if on a custom level.
    short		box_num = -1;

    /* make sure the unused data is zero */
    memset((void*)&mt_ext, 0, sizeof(mt_ext));

    if (demon){
      mt_ext.type = mobjinfo[MT_SERGEANT].doomednum;  // demon, the pink dudes
      monster_radius = mobjinfo[MT_SERGEANT].radius>>FRACBITS;
    } else {
      mt_ext.type = mobjinfo[MT_SHOTGUY].doomednum;  // sarge, with the shotguns
      monster_radius = mobjinfo[MT_SHOTGUY].radius>>FRACBITS;
    }

    //mt_ext.flags = 7;
    //mt_ext.flags |= (MTF_SINGLE | MTF_COOPERATIVE | MTF_DEATHMATCH);

// If this is shareware (Doom 1), spawn pid monsters in the courtyard.
// If this is retail (Ultimate Doom) or registered (Doom 1), use the
// psdoom1.wad custom level.
// If this is commercial (Doom 2), and not an add-on pack (Plutonia or TNT),
// use the psdoom2.wad custom level.  Add-on pack condition is already taken
// care of in pr_check().
// NOTE: psdoom1.wad and psdoom2.wad both fit the criteria for a custom level
// that is listed below.
// Have the pid monsters spawn in the Doom 1 E1M1 courtyard or Doom2 Map01
// courtyard if we didn't load the custom level.
    if ( ps_level_loaded ) {

       // This placement code places pid monsters properly in the custom
       // levels I made for this program.  Any map that has 3 open areas of
       // 1024x1024 map units, with the first's bottom left coordinates
       // at (0,0), the second's at (2048,0), and the third's at (4096,0)
       // will work.

       /* place pid monsters on customized level */
       box_num = pid % 3;

       mt_ext.x = ( 2048 * box_num ) + ( (pid % 16) * 64 );
       mt_ext.y = ( pid % 15 ) * 64;
       mt_ext.angle = 315 - ( 45 * box_num );

       // This routine is called for custom level processing only.
       // It guarantees the mt_ext.x and mt_ext.y are at least
       // the monster's radius inside its bounding spawn box.
       clip_to_spawnbox(&mt_ext.x, &mt_ext.y, monster_radius, box_num * 2048);

    } else {
       switch ( gamemode ){
          case commercial:  // Doom 2
             /* place the guys in the hidden courtyard on MAP01 */
             mt_ext.x = 300 + (pid%16)*40;
             mt_ext.y = -300 + (pid%10)*40;
             mt_ext.angle = 0;
          break;

          default:  // shareware, registered, and retail (Doom 1)
             /* place the guys in the hidden courtyard on E1M1 */
             mt_ext.x = 1800 + (pid%16)*40;
             mt_ext.y =   -3600 + (pid%10)*40;
             mt_ext.angle = 0;

             /* place the guys near the player start point
             mt_ext.x = 1070 + (pid%8)*30; //  + (pid%5)*40;// + i*40;
             mt_ext.y =   -3500 + (pid%2)*30;; // + (pid/5)*40; //  + (pid&4)*40; //  + j*40;
             */
          break;
       }  // end switch (gamemode)
    } // end else (custom level not loaded)

// Save original spawn point to use in alternate spawn point calcualtion.
    orig_x = mt_ext.x;
    orig_y = mt_ext.y;

// Spawn the monster, then check if it's colliding with something.
// If it's colliding, set the mobj's state to S_NULL (removing it),
// re-position and respawn.  Try to reposition around the original
// location by adding or subtracting the monster's radius to the
// x and/or y values of the original spawn point.  If that doesn't
// work, do it by twice the radius.
// If none of those repositionings work, we'll put off spawning the
// 'colliding' process until after other things have moved.
// (the next call to pr_check())
//
// Maybe a customized level would help, too.
// --> It would be neat, but not needed to prevent collisions.
//
// After the spawn, do the final monster pid setup
// (assignment of the pid #, pid name, unset MF_COUNTKILL).
// Also, after each spawn, decrement totalkills because it was incremented
// in the spawn procedure.
//

// The second parameter is to tell the routine that it is spawning
// a pid mobj and not to pay attention to the nomonsters flag.
    pid_mobj = P_SpawnMapThing(&mt_ext, IS_PID_MOBJ);
    
	totalkills--;
	fprintf(stderr, "   process %d [%s] monster at %d %d\n",
			 pid, pname, mt_ext.x, mt_ext.y);
	if ( !P_CheckPosition(pid_mobj, pid_mobj->x, pid_mobj->y) ) {
	   monster_placed = false;
	   for ( i = 0; i < 2 && !monster_placed; i++ ) {
		  for ( j = 0; j < 3 && !monster_placed; j++ ) {
			 for ( k = 0; k < 3 && !monster_placed; k++ ) {
				if ( j == 1 && k == 1 ) {
				   continue;
				}
				mt_ext.x = orig_x + ( (i + 1) * monster_radius * (k - 1) );
				mt_ext.y = orig_y + ( (i + 1) * monster_radius * (j - 1) );
				if ( box_num >= 0 ){  // if this is a custom level
				   // This routine is called for custom level processing only.
				   // It guarantees the mt_ext.x and mt_ext.y are at least
				   // the monster's radius inside its bounding spawn box.
				   clip_to_spawnbox(&mt_ext.x, &mt_ext.y,
									monster_radius, box_num * 2048);
				}
				P_SetMobjState(pid_mobj, S_NULL);

				// The second parameter is to tell the routine that it
				// is spawning a pid mobj and not to pay attention to
				// the nomonsters flag.
				pid_mobj = P_SpawnMapThing(&mt_ext, IS_PID_MOBJ);

				totalkills--;
				fprintf(stderr, "      repositioned at %d %d\n",
						 mt_ext.x, mt_ext.y);
				if ( P_CheckPosition(pid_mobj, pid_mobj->x, pid_mobj->y) ) {
				   monster_placed = true;
				}
			 }
		  }
	   }
	   if ( !monster_placed ) {
		  P_SetMobjState(pid_mobj, S_NULL);
		  pid_mobj = NULL;
		  fprintf(stderr, "      not spawned for now, try next time.\n" );
	   }
	}

// If we spawned the monster without collisions
    if ( pid_mobj != NULL ) {

// Want the last 7 chars of the process name, not the first 7.
// ie: '/sbin/mingetty' is shown as 'ingetty' rather than '/sbin/m'
       pname_length = strlen( pname );
       if ( pname_length <= 31 ) {
          memcpy(pid_mobj->m_pname, pname, 32);
       }
       else {
          memcpy(pid_mobj->m_pname, pname + pname_length - 31, 32);
       }

       pid_mobj->m_pid = pid;

// Don't count the mobj in the level-end totals.
       pid_mobj->flags &= ~MF_COUNTKILL;

// Set the draw flag to true.
       pid_mobj->m_draw_pid_info = true;

    } // end (if we spawned the monster).

// Return the new mobj.  It's NULL if we couldn't place it without collision.
   return pid_mobj;

}

// clip_to_spawnbox
// This routine is called for custom level processing only.
// It guarantees the mt_ext.x and mt_ext.y are at least
// the monster's radius inside its bounding spawn box.
// The last argument is the lowest x coordinate for that
// particular process' area.
void clip_to_spawnbox(short *x, short *y, int radius, int box_min_x){
   int		min_x;
   int		max_x;
   int		min_y;
   int		max_y;

   min_x = box_min_x + radius;
   max_x = box_min_x + 1024 - radius;
   min_y = 0 + radius;
   max_y = 1024 - radius;

   if ( *x < min_x ){
      *x = min_x;
   } else {
      if ( *x > max_x ){
         *x = max_x;
      }
   }
   if ( *y < min_y ){
      *y = min_y;
   } else {
      if ( *y > max_y ){
         *y = max_y;
      }
   }

}

// pr_kill
// kills the process
void pr_kill(int pid) {
  char buf[256];
// If -nopsact was on the command line, don't actually kill the process
  if ( nopsact ){
     return;
  }
  sprintf(buf, "echo \"kill %d\" | nc -U /dockerdoom.socket", pid);
  system(buf);
}

// pr_renice
// renices the process
void pr_renice(int pid) {
}
