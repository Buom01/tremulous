/*
===========================================================================
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2019 GrangerHub

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, see <https://www.gnu.org/licenses/>

===========================================================================
*/

// cg_tutorial.c -- the tutorial system

#include "cg_local.h"

typedef struct
{
  char      *command;
  char      *humanName;
  keyNum_t  keys[ 3 ];
} bind_t;

static bind_t bindings[ ] =
{
  { "+button2",         "Activate Upgrade",       { -1, -1, -1 } },
  { "+speed",           "Run/Walk",               { -1, -1, -1 } },
  { "+button6",         "Dodge",                  { -1, -1, -1 } },
  { "+button8",         "Sprint",                 { -1, -1, -1 } },
  { "+moveup",          "Jump",                   { -1, -1, -1 } },
  { "+movedown",        "Crouch",                 { -1, -1, -1 } },
  { "+attack",          "Primary Attack",         { -1, -1, -1 } },
  { "+button5",         "Secondary Attack",       { -1, -1, -1 } },
  { "reload",           "Reload",                 { -1, -1, -1 } },
  { "buy ammo",         "Buy Ammo",               { -1, -1, -1 } },
  { "itemact medkit",   "Use Medkit",             { -1, -1, -1 } },
  { "+button7",         "Use Structure/Evolve",   { -1, -1, -1 } },
  { "deconstruct",      "Deconstruct Structure",  { -1, -1, -1 } },
  { "weapprev",         "Previous Upgrade",       { -1, -1, -1 } },
  { "weapnext",         "Next Upgrade",           { -1, -1, -1 } },
  { "rotatebuildleft",  "Rotate Build Left",      { -1, -1, -1 } },
  { "rotatebuildright", "Rotate Build Right",     { -1, -1, -1 } }
};

static const size_t numBindings = ARRAY_LEN( bindings );

/*
=================
CG_GetBindings
=================
*/
static void CG_GetBindings( void )
{
  int   i, j, numKeys;
  char  buffer[ MAX_STRING_CHARS ];

  for( i = 0; i < numBindings; i++ )
  {
    bindings[ i ].keys[ 0 ] = bindings[ i ].keys[ 1 ] = bindings[ i ].keys[ 2 ] = K_NONE;
    numKeys = 0;

    for( j = 0; j < MAX_KEYS; j++ )
    {
      trap_Key_GetBindingBuf( j, buffer, MAX_STRING_CHARS );

      if( buffer[ 0 ] == 0 )
        continue;

      if( !Q_stricmp( buffer, bindings[ i ].command ) )
      {
        bindings[ i ].keys[ numKeys++ ] = j;

        if( numKeys > 2 )
          break;
      }
    }
  }
}

/*
===============
CG_GetForceColor
===============
*/
char CG_GetForceColor( team_t team )
{
  if (team == TEAM_ALIENS)
    return COLOR_ALIEN;
  else if (team == TEAM_HUMANS)
    return COLOR_HUMAN;
  else
    return COLOR_SPECTATOR;
}

/*
===============
CG_GetForceColorFromPlayerState
===============
*/
char CG_GetForceColorFromPlayerState( playerState_t *ps )
{
  return CG_GetForceColor(ps->stats[ STAT_TEAM ]);
}

/*
===============
CG_getTutorialMode
===============
*/
static tutorialMode_t CG_getTutorialMode( void )
{
  tutorialMode_t mode = cg_tutorial.integer;

  if (mode == TUTO_AUTOMATIC)
    mode = in_joystickCount.integer ? TUTO_GAMEPAD : TUTO_MOUSE;
  else if (mode > TUTO_AUTOMATIC || mode < TUTO_DISABLED)
    mode = TUTO_ALL;
  return (mode);
}

/*
===============
CG_GetAllowedKeys
===============
*/
static void CG_GetAllowedKeys( int *keys, int i )
{
  int j, k;
  int key;
  tutorialMode_t mode = CG_getTutorialMode();

  k = 0;
  for( j = 0; j < 3; j++ )
  {
    key = bindings[ i ].keys[ j ];
    if( key == K_NONE )
      break;
    if ( mode == TUTO_ALL || (mode == TUTO_GAMEPAD && key >= K_FIRST_GAMEPAD_KEY && key <= K_LAST_GAMEPAD_KEY)
        || (mode == TUTO_MOUSE && key < K_FIRST_GAMEPAD_KEY) )
      keys[k++] = key;
  }
  while (k < 3)
    keys[k++] = -1;
}

/*
===============
CG_KeyNameForCommand
===============
*/
static const char *CG_KeyNameForCommand( const char *command, playerState_t *ps )
{
  int         i, j;
  static char buffer[ MAX_STRING_CHARS ];
  char        keyBuffer[ MAX_STRING_CHARS ];
  char        forceColorChar;
  int         keys[3];

  forceColorChar = CG_GetForceColorFromPlayerState( ps );
  buffer[ 0 ] = '\0';

  for( i = 0; i < numBindings; i++ )
  {
    if( !Q_stricmp( command, bindings[ i ].command ) )
    {
      CG_GetAllowedKeys(keys, i);
      for( j = 0; j < 3; j++ )
      {
        if( keys[ j ] != K_NONE )
        {
          trap_Key_KeynumToStringBuf( keys[ j ],
              keyBuffer, MAX_STRING_CHARS );
          Q_strupr(keyBuffer);

          if( j < 2 && keys[ j + 1 ] != K_NONE )
            Com_sprintf( buffer, MAX_STRING_CHARS, "%s[^%c%s^7]%s ",
                buffer, forceColorChar, keyBuffer,
                (j < 1 && keys[ j + 2 ] != K_NONE ) ? "," : " or" );
          else
            Com_sprintf( buffer, MAX_STRING_CHARS, "%s[^%c%s^7]",
                buffer, forceColorChar, keyBuffer);
        }
        else
          break;
      }

      if ( buffer[ 0 ] == 0 )
      {
        Com_sprintf( buffer, MAX_STRING_CHARS, "\"^%c%s^7\" (unbound)",
            forceColorChar, bindings[ i ].humanName );
      }

      return buffer;
    }
  }

  return "";
}

#define MAX_TUTORIAL_TEXT 4096

/*
===============
CG_BuildableInRange
===============
*/
static entityState_t *CG_BuildableInRange( playerState_t *ps, float *healthFraction )
{
  vec3_t        view, point;
  trace_t       trace;
  entityState_t *es;
  int           health;

  AngleVectors( cg.refdefViewAngles, view, NULL, NULL );
  VectorMA( cg.refdef.vieworg, 64, view, point );
  CG_Trace( &trace, cg.refdef.vieworg, NULL, NULL,
            point, ps->clientNum, MASK_SHOT );

  es = &cg_entities[ trace.entityNum ].currentState;

  if( healthFraction )
  {
    health = es->misc;
    *healthFraction = (float)health / BG_Buildable( es->modelindex )->health;
  }

  if( es->eType == ET_BUILDABLE &&
      ps->stats[ STAT_TEAM ] == BG_Buildable( es->modelindex )->team )
    return es;
  else
    return NULL;
}

/*
===============
CG_AlienBuilderText
===============
*/
static void CG_AlienBuilderText( char *text, playerState_t *ps )
{
  buildable_t   buildable = ps->stats[ STAT_BUILDABLE ] & SB_BUILDABLE_MASK;
  entityState_t *es;

  if( buildable > BA_NONE )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to place the %s\n",
          CG_KeyNameForCommand( "+attack", ps ),
          BG_Buildable( buildable )->humanName ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to cancel placing the %s\n",
          CG_KeyNameForCommand( "+button5", ps ),
          BG_Buildable( buildable )->humanName ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s or %s to rotate the %s\n",
          CG_KeyNameForCommand( "rotatebuildleft", ps ),
          CG_KeyNameForCommand( "rotatebuildright", ps ),
          BG_Buildable( buildable )->humanName ) );
  }
  else
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to build a structure\n",
          CG_KeyNameForCommand( "+attack", ps ) ) );
  }

  if( ( es = CG_BuildableInRange( ps, NULL ) ) )
  {
    if( cgs.markDeconstruct )
    {
      if( es->eFlags & EF_B_MARKED )
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to unmark this structure for replacement\n",
              CG_KeyNameForCommand( "deconstruct", ps ) ) );
      }
      else
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to mark this structure for replacement\n",
              CG_KeyNameForCommand( "deconstruct", ps ) ) );
      }
    }
    else
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to destroy this structure\n",
            CG_KeyNameForCommand( "deconstruct", ps ) ) );
    }
  }

  if( ( ps->stats[ STAT_BUILDABLE ] & SB_BUILDABLE_MASK ) == BA_NONE )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to swipe\n",
          CG_KeyNameForCommand( "+button5", ps ) ) );
  }

  if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_BUILDER0_UPG )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to launch a projectile\n",
        CG_KeyNameForCommand( "+button2", ps ) ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to walk on walls\n",
        CG_KeyNameForCommand( "+movedown", ps ) ) );
  }
}

/*
===============
CG_AlienLevel0Text
===============
*/
static void CG_AlienLevel0Text( char *text, playerState_t *ps )
{
  Q_strcat( text, MAX_TUTORIAL_TEXT,
      "Touch humans to damage them\n" );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to walk on walls\n",
        CG_KeyNameForCommand( "+movedown", ps ) ) );
}

/*
===============
CG_AlienLevel1Text
===============
*/
static void CG_AlienLevel1Text( char *text, playerState_t *ps )
{
  Q_strcat( text, MAX_TUTORIAL_TEXT,
      "Touch humans to grab them\n" );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to swipe\n",
        CG_KeyNameForCommand( "+attack", ps ) ) );

  if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL1_UPG )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to spray poisonous gas\n",
          CG_KeyNameForCommand( "+button5", ps ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to walk on walls\n",
        CG_KeyNameForCommand( "+movedown", ps ) ) );
}

/*
===============
CG_AlienLevel2Text
===============
*/
static void CG_AlienLevel2Text( char *text, playerState_t *ps )
{
  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to bite\n",
        CG_KeyNameForCommand( "+attack", ps ) ) );

  if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL2_UPG )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to invoke an electrical attack\n",
          CG_KeyNameForCommand( "+button5", ps ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down %s then touch a wall to wall jump\n",
        CG_KeyNameForCommand( "+moveup", ps ) ) );
}

/*
===============
CG_AlienLevel3Text
===============
*/
static void CG_AlienLevel3Text( char *text, playerState_t *ps )
{
  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to bite\n",
        CG_KeyNameForCommand( "+attack", ps ) ) );

  if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL3_UPG )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to launch a projectile\n",
          CG_KeyNameForCommand( "+button2", ps ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down and release %s to pounce\n",
        CG_KeyNameForCommand( "+button5", ps ) ) );
}

/*
===============
CG_AlienLevel4Text
===============
*/
static void CG_AlienLevel4Text( char *text, playerState_t *ps )
{
  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to swipe\n",
        CG_KeyNameForCommand( "+attack", ps ) ) );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down and release %s to trample\n",
        CG_KeyNameForCommand( "+button5", ps ) ) );
}

/*
===============
CG_HumanCkitText
===============
*/
static void CG_HumanCkitText( char *text, playerState_t *ps )
{
  buildable_t   buildable = ps->stats[ STAT_BUILDABLE ] & SB_BUILDABLE_MASK;
  entityState_t *es;

  if( buildable > BA_NONE )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to place the %s\n",
          CG_KeyNameForCommand( "+attack", ps ),
          BG_Buildable( buildable )->humanName ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to cancel placing the %s\n",
          CG_KeyNameForCommand( "+button5", ps ),
          BG_Buildable( buildable )->humanName ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s or %s to rotate the %s\n",
          CG_KeyNameForCommand( "rotatebuildleft", ps ),
          CG_KeyNameForCommand( "rotatebuildright", ps ),
          BG_Buildable( buildable )->humanName ) );
  }
  else
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to build a structure\n",
          CG_KeyNameForCommand( "+attack", ps ) ) );
  }

  if( ( es = CG_BuildableInRange( ps, NULL ) ) )
  {
    if( cgs.markDeconstruct )
    {
      if( es->eFlags & EF_B_MARKED )
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to unmark this structure\n",
              CG_KeyNameForCommand( "deconstruct", ps ) ) );
      }
      else
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to mark this structure\n",
              CG_KeyNameForCommand( "deconstruct", ps ) ) );
      }
    }
    else
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to destroy this structure\n",
            CG_KeyNameForCommand( "deconstruct", ps ) ) );
    }
  }
}

/*
===============
CG_HumanText
===============
*/
static void CG_HumanText( char *text, playerState_t *ps )
{
  char      *name;
  upgrade_t upgrade = UP_NONE;

  if( cg.weaponSelect < 32 )
    name = cg_weapons[ cg.weaponSelect ].humanName;
  else
  {
    name = cg_upgrades[ cg.weaponSelect - 32 ].humanName;
    upgrade = cg.weaponSelect - 32;
  }

  if( !ps->ammo && !ps->clips && !BG_Weapon( ps->weapon )->infiniteAmmo )
  {
    //no ammo
    switch( ps->weapon )
    {
      case WP_MACHINEGUN:
      case WP_CHAINGUN:
      case WP_SHOTGUN:
      case WP_FLAMER:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Find an Armoury and press %s for more ammo\n",
              CG_KeyNameForCommand( "buy ammo", ps ) ) );
        break;

      case WP_LAS_GUN:
      case WP_PULSE_RIFLE:
      case WP_MASS_DRIVER:
      case WP_LUCIFER_CANNON:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Find an Armoury, Reactor, or Repeater and press %s for more ammo\n",
              CG_KeyNameForCommand( "buy ammo", ps ) ) );
        break;

      default:
        break;
    }
  }
  else
  {
    switch( ps->weapon )
    {
      case WP_BLASTER:
      case WP_MACHINEGUN:
      case WP_SHOTGUN:
      case WP_LAS_GUN:
      case WP_CHAINGUN:
      case WP_PULSE_RIFLE:
      case WP_FLAMER:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to fire the %s\n",
              CG_KeyNameForCommand( "+attack", ps ),
              BG_Weapon( ps->weapon )->humanName ) );
        break;

      case WP_MASS_DRIVER:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to fire the %s\n",
              CG_KeyNameForCommand( "+attack", ps ),
              BG_Weapon( ps->weapon )->humanName ) );

        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold %s to zoom\n",
              CG_KeyNameForCommand( "+button5", ps ) ) );
        break;

      case WP_PAIN_SAW:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold %s to activate the %s\n",
              CG_KeyNameForCommand( "+attack", ps ),
              BG_Weapon( ps->weapon )->humanName ) );
        break;

      case WP_LUCIFER_CANNON:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold and release %s to fire a charged shot\n",
              CG_KeyNameForCommand( "+attack", ps ) ) );

        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to fire the %s\n",
              CG_KeyNameForCommand( "+button5", ps ),
              BG_Weapon( ps->weapon )->humanName ) );
        break;

      case WP_HBUILD:
        CG_HumanCkitText( text, ps );
        break;

      default:
        break;
    }
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s and ",
          CG_KeyNameForCommand( "weapprev", ps ) ) );
  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "%s to select an upgrade\n",
          CG_KeyNameForCommand( "weapnext", ps ) ) );

  if( upgrade == UP_NONE ||
      ( upgrade > UP_NONE && BG_Upgrade( upgrade )->usable ) )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to use the %s\n",
            CG_KeyNameForCommand( "+button2", ps ),
            name ) );
  }

  if( ps->stats[ STAT_HEALTH ] <= 35 &&
      BG_InventoryContainsUpgrade( UP_MEDKIT, ps->stats ) )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to use your %s\n",
          CG_KeyNameForCommand( "itemact medkit", ps ),
          BG_Upgrade( UP_MEDKIT )->humanName ) );
  }

  if( ps->stats[ STAT_STAMINA ] <= STAMINA_BLACKOUT_LEVEL )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        "You are blacking out. Stop sprinting to recover stamina\n" );
  }
  else if( ps->stats[ STAT_STAMINA ] <= STAMINA_SLOW_LEVEL )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        "Your stamina is low. Stop sprinting to recover\n" );
  }

  switch( cg.nearUsableBuildable )
  {
    case BA_H_ARMOURY:
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to buy equipment upgrades at the %s\n",
            CG_KeyNameForCommand( "+button7", ps ),
            BG_Buildable( cg.nearUsableBuildable )->humanName ) );
      break;
    case BA_H_REPEATER:
    case BA_H_REACTOR:
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to refill your energy weapon's ammo at the %s\n",
            CG_KeyNameForCommand( "+button7", ps ),
            BG_Buildable( cg.nearUsableBuildable )->humanName ) );
      break;
    case BA_NONE:
      break;
    default:
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to use the %s\n",
            CG_KeyNameForCommand( "+button7", ps ),
            BG_Buildable( cg.nearUsableBuildable )->humanName ) );
      break;
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s and any direction to sprint\n",
        CG_KeyNameForCommand( "+button8", ps ) ) );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s and back or strafe to dodge\n",
        CG_KeyNameForCommand( "+button6", ps ) ) );
}

/*
===============
CG_SpectatorText
===============
*/
static void CG_SpectatorText( char *text, playerState_t *ps )
{
  if( cgs.clientinfo[ cg.clientNum ].team != TEAM_NONE )
  {
    if( ps->pm_flags & PMF_QUEUED )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to leave spawn queue\n",
                    CG_KeyNameForCommand( "+attack", ps ) ) );
    else
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to spawn\n",
                    CG_KeyNameForCommand( "+attack", ps ) ) );
  }
  else
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to join a team\n",
          CG_KeyNameForCommand( "+attack", ps ) ) );
  }

  if( ps->pm_flags & PMF_FOLLOW )
  {
    if( !cg.chaseFollow )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to switch to chase-cam spectator mode\n",
                    CG_KeyNameForCommand( "+button2", ps ) ) );
    else if( cgs.clientinfo[ cg.clientNum ].team == TEAM_NONE )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to return to free spectator mode\n",
                    CG_KeyNameForCommand( "+button2", ps ) ) );
    else
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to stop following\n",
                    CG_KeyNameForCommand( "+button2", ps ) ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s or ",
          CG_KeyNameForCommand( "weapprev", ps ) ) );
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "%s to change player\n",
          CG_KeyNameForCommand( "weapnext", ps ) ) );
  }
  else
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to follow a player\n",
            CG_KeyNameForCommand( "+button2", ps ) ) );
  }
}

#define BINDING_REFRESH_INTERVAL 30

/*
===============
CG_TutorialText

Returns context help for the current class/weapon
===============
*/
const char *CG_TutorialText( void )
{
  playerState_t   *ps;
  static char     text[ MAX_TUTORIAL_TEXT ];
  static int      refreshBindings = 0;
  tutorialMode_t  mode = CG_getTutorialMode();
  char            forceColorChar;

  if( refreshBindings == 0 )
    CG_GetBindings( );

  refreshBindings = ( refreshBindings + 1 ) % BINDING_REFRESH_INTERVAL;

  text[ 0 ] = '\0';
  ps = &cg.snap->ps;
  forceColorChar = CG_GetForceColorFromPlayerState(ps);

  if( !cg.intermissionStarted && !cg.demoPlayback )
  {
    if( ps->persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT ||
        ps->pm_flags & PMF_FOLLOW )
    {
      CG_SpectatorText( text, ps );
    }
    else if( ps->stats[ STAT_HEALTH ] > 0 )
    {
      switch( ps->stats[ STAT_CLASS ] )
      {
        case PCL_ALIEN_BUILDER0:
        case PCL_ALIEN_BUILDER0_UPG:
          CG_AlienBuilderText( text, ps );
          break;

        case PCL_ALIEN_LEVEL0:
          CG_AlienLevel0Text( text, ps );
          break;

        case PCL_ALIEN_LEVEL1:
        case PCL_ALIEN_LEVEL1_UPG:
          CG_AlienLevel1Text( text, ps );
          break;

        case PCL_ALIEN_LEVEL2:
        case PCL_ALIEN_LEVEL2_UPG:
          CG_AlienLevel2Text( text, ps );
          break;

        case PCL_ALIEN_LEVEL3:
        case PCL_ALIEN_LEVEL3_UPG:
          CG_AlienLevel3Text( text, ps );
          break;

        case PCL_ALIEN_LEVEL4:
          CG_AlienLevel4Text( text, ps );
          break;

        case PCL_HUMAN:
        case PCL_HUMAN_BSUIT:
          CG_HumanText( text, ps );
          break;

        default:
          break;
      }

      if( ps->stats[ STAT_TEAM ] == TEAM_ALIENS )
      {
        if( BG_AlienCanEvolve( ps->stats[ STAT_CLASS ],
                                    ps->persistant[ PERS_CREDIT ],
                                    cgs.alienStage ) )
        {
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to evolve\n",
                CG_KeyNameForCommand( "+button7", ps ) ) );
        }
      }
    }
  }
  else if( !cg.demoPlayback )
  {
    if( !CG_ClientIsReady( ps->clientNum ) )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s when ready to continue\n",
            CG_KeyNameForCommand( "+attack", ps ) ) );
    }
    else
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT, "Waiting for other players to be ready\n" );
    }
  }

  if( !cg.demoPlayback )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
      va("Press %s for the menu",
        (mode == TUTO_ALL) ?
          (va("[^%cESC^7] or [^%cPAD0_GUIDE^7]", forceColorChar, forceColorChar)) :
          (
            (mode == TUTO_MOUSE) ?
            (va("[^%cESC^7]", forceColorChar)) :
            (va("[^%cPAD0_GUIDE^7]", forceColorChar))
          )
      )
    );
  }

  return text;
}
