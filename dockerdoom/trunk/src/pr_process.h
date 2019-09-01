// pr_process.h
// Copyright (C) 1999 by Dennis Chao
// Copyright (C) 2000 by David Koppenhofer

// Passed as second argument to P_SpawnMapThing() to tell it
// if we're spawning a pid mobj or not.
#define IS_NOT_PID_MOBJ		0
#define IS_PID_MOBJ		1

// All flags below are generally set up in d_main.c, D_DoomMain().

// Need to have a flag to tell if we loaded the special process
// management level.
extern boolean ps_level_loaded;

// Flag to prevent the 'ps' portion of the program from running.
extern boolean nopsmon;

// Flag to prevent the execution of process re-nicing and killing.
extern boolean nopsact;

// Flag to allow pid monsters to be damaged by things other than a player.
extern boolean nopssafety;

// Flag to show all users' processes.
extern boolean psallusers;

// Enumeration of types of user lists to have.
// Passed to and used in add_to_ps_userlist().
typedef enum
{
   psuser,
   psnotuser
} ps_userlist_type_t;

// Functions
void pr_check(void);
void cleanup_pid_list(mobj_t *del_mobj);
void pr_kill(int pid);
void pr_renice(int pid);
void add_to_ps_userlist(ps_userlist_type_t listtype, char *username);
