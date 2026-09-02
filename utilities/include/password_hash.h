/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-26
Module: Utilities
File: password_hash.h
About: Salted SHA-256 password hashing for pwd.bd storage.
       Replaces the old plaintext password storage — passwords
       are no longer stored or compared as plaintext.

       Stored format written to pwd.bd: "saltHex:digestHex"
         saltHex   = 16 hex chars (8-byte random salt)
         digestHex = 64 hex chars (32-byte SHA-256 digest of salt+password)

       The special sentinel string "0" (used elsewhere in
       user_management to mean "no password set") is NOT hashed —
       callers must keep checking for it before calling
       password_verify()/instead of password_hash().
Revisions:
- 2026-08-26  Initial implementation
------------------------------------------------------------
*/

#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

/* "saltHex(16):digestHex(64)" + NUL. Buffers that hold a value read
   from/written to pwd.bd must be at least this large. */
#define PASSWORD_HASH_LEN 82

/*
Hashes `password` with a freshly generated random salt and writes
"saltHex:digestHex" into `out` (out must be >= PASSWORD_HASH_LEN bytes).
*/
void password_hash(const char* password, char* out);

/*
Returns 1 if `password` matches the salted hash in `stored` (the
"saltHex:digestHex" string as produced by password_hash()), else 0.
*/
int password_verify(const char* password, const char* stored);

#endif
