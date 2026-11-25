# Dungeon Delve 0.20.4 - Steam invites

Invite Steam friends instead of sharing an IP address. With Steam running, the
host's open seats show **INVITE** cards that open Steam's invite dialog. Friends
can accept the invite, use **Join Game** on the host in their Steam friends list,
or paste the copied invite code (`steam:<number>`) into the Direct IP box. Invited
players connect through Steam's relay, so the host needs no port forwarding.
Direct IP still works exactly as before.

Important: friends must already have Dungeon Delve open (from Dungeon Delve.exe)
when they accept. Steam starts its normal Barony for invites accepted while the
game is closed, and normal Barony cannot join; it is refused cleanly.

Steam invites passed automated multi-player tests and a single real-account
check (real Steam lobby and invite dialog). They have not yet been tested between
two real Steam accounts, so please report any problems; Direct IP remains the
fallback. Gameplay, gates, loot and the three floors are unchanged from 0.20.3.
All peers need runtime/content 0.20.4, protocol 35 and API 1. Restart from the
root Dungeon Delve.exe.
