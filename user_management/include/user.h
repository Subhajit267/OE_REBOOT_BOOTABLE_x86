/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-22
Module: User Management
File: user.h
About: Original OE user management system converted
       to C with binary file storage.
Revisions:
- 2026-02-21  Switched to binary storage (user.bd, pwd.bd)
- 2026-02-21  Removed C++ dependencies
- 2026-02-22  Added user_enter_guest() and user_exists()
- 2026-03-16  reg_status renamed to is_guest_mode, future model placeholder added
------------------------------------------------------------
*/

#ifndef USER_H
#define USER_H

/* -------- FILES -------- */
#define USER_FILE  "user.bd"
#define PASS_FILE  "pwd.bd"

/* -------- LIMITS -------- */
#define USERNAME_MAX 64
#define PASSWORD_MAX 64

/* -------- GLOBAL STATE -------- */
extern char current_user[USERNAME_MAX];
extern int is_guest_mode;  /* 0 = normal user, 1 = guest */

/* -------- ORIGINAL OE FUNCTIONS -------- */
void login(void);
void add_user(void);
void password_change(void);
void userid_change(void);

/* -------- GUEST MODE -------- */
void user_enter_guest(void);

/* -------- UTILITY -------- */
int user_exists(void);


/* ================================================================
   FUTURE USER MODEL -- Phase 2 placeholder (currently inactive)
   To be implemented when OEFS filesystem is ready.
   Uncomment and implement when phase 2 begins.
   ================================================================

#define UID_SYSTEM   0
#define UID_ADMIN    1
#define UID_GUEST    255

typedef struct {
    uint16_t uid;
    char     username[32];
    uint8_t  user_type;      // 0=system 1=admin 2=normal 255=guest
    uint8_t  flags;          // 0x01=locked 0x02=no_password
    uint32_t created_time;
    uint32_t last_login;
} OE_UserEntry;

typedef struct {
    uint16_t real_uid;
    uint16_t effective_uid;  // elevated during admin session
    uint32_t elevated_since;
    uint32_t elevation_timeout;
} OE_Session;

void user_elevate_to_admin(void);
void user_drop_elevation(void);
int  user_is_admin(void);

================================================================ */

#endif