/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file npc.c
 *
 * @brief Handles NPC stuff.
 */


#include "npc.h"

#include "naev.h"

#include <string.h>

#include "log.h"
#include "land.h"
#include "opengl.h"
#include "array.h"
#include "dialogue.h"
#include "event.h"


/**
 * @brief NPC types.
 */
typedef enum NPCtype_ {
   NPC_TYPE_NULL,
   NPC_TYPE_GIVER,
   NPC_TYPE_MISSION,
   NPC_TYPE_EVENT
} NPCtype;

/**
 * @brief Minimum needed NPC data for event.
 */
typedef struct NPCevtData_ {
   unsigned int id;
   char *func;
} NPCevtData;
/**
 * @brief Minimum needed NPC data for mission.
 */
typedef struct NPCmisnData_ {
   Mission *misn;
   char *func;
} NPCmisnData;
/**
 * @brief The bar NPC.
 */
typedef struct NPC_s {
   unsigned int id;
   NPCtype type;
   int priority;
   char *name;
   glTexture *portrait;
   char *desc;
   union {
      Mission g;
      NPCmisnData m;
      NPCevtData e;
   } u;
} NPC_t;

static unsigned int npc_array_idgen = 0; /**< ID generator. */
static NPC_t *npc_array  = NULL; /**< Missions at the spaceport bar. */


/*
 * Prototypes.
 */
static unsigned int npc_add( NPC_t *npc );
static unsigned int npc_add_giver( Mission *misn );
static int npc_rm( NPC_t *npc );
static void npc_sort (void);
static NPC_t *npc_arrayGet( unsigned int id );
static void npc_free( NPC_t *npc );


/**
 * @brief Adds an NPC to the spaceport bar.
 */
static unsigned int npc_add( NPC_t *npc )
{
   NPC_t *new_npc;

   /* Must be landed. */
   if (!landed)
      return 0;

   /* Create if needed. */
   if (npc_array == NULL)
      npc_array = array_create( NPC_t );

   /* Grow. */
   new_npc = &array_grow( &npc_array );

   /* Copy over. */
   memcpy( new_npc, npc, sizeof(NPC_t) );

   /* Set ID. */
   new_npc->id = ++npc_array_idgen;
   return new_npc->id;
}


/**
 * @brief Adds a mission giver NPC to the mission computer.
 */
static unsigned int npc_add_giver( Mission *misn )
{
   NPC_t npc;

   /* Set up the data. */
   npc.type       = NPC_TYPE_GIVER;
   npc.name       = strdup(misn->npc);
   npc.priority   = misn->data->avail.priority;
   npc.portrait   = gl_dupTexture(misn->portrait);
   npc.desc       = strdup(misn->desc);
   memcpy( &npc.u.g, misn, sizeof(Mission) );

   return npc_add( &npc );
}


/**
 * @brief Adds a mission NPC to the mission computer.
 */
unsigned int npc_add_mission( Mission *misn, const char *func, const char *name,
      int priority, const char *portrait, const char *desc )
{
   NPC_t npc;

   /* The data. */
   npc.type       = NPC_TYPE_MISSION;
   npc.name       = strdup( name );
   npc.priority   = priority;
   npc.portrait   = gl_newImage( portrait, 0 );
   npc.desc       = strdup( desc );
   npc.u.m.misn   = misn;
   npc.u.m.func   = strdup( func );

   return npc_add( &npc );
}


/**
 * @brief Adds a event NPC to the mission computer.
 */
unsigned int npc_add_event( unsigned int evt, const char *func, const char *name,
      int priority, const char *portrait, const char *desc )
{
   NPC_t npc;

   /* The data. */
   npc.type       = NPC_TYPE_EVENT;
   npc.name       = strdup( name );
   npc.priority   = priority;
   npc.portrait   = gl_newImage( portrait, 0 );
   npc.desc       = strdup( desc );
   npc.u.e.id     = evt;
   npc.u.e.func   = strdup( func );

   return npc_add( &npc );
}


/**
 * @brief Removes an npc from the spaceport bar.
 */
static int npc_rm( NPC_t *npc )
{
   npc_free(npc);

   array_erase( &npc_array, &npc[0], &npc[1] );

   return 0;
}


/**
 * @brief Gets an NPC by ID.
 */
static NPC_t *npc_arrayGet( unsigned int id )
{
   int i;
   for (i=0; i<array_size( npc_array ); i++)
      if (npc_array[i].id == id)
         return &npc_array[i];
   return NULL;
}


/**
 * @brief removes an event NPC.
 */
int npc_rm_event( unsigned int id, unsigned int evt )
{
   NPC_t *npc;

   /* Get the NPC. */
   npc = npc_arrayGet( id );
   if (npc == NULL)
      return -1;

   /* Doesn't match type. */
   if (npc->type != NPC_TYPE_EVENT)
      return -1;

   /* Doesn't belong to the event.. */
   if (npc->u.e.id != evt)
      return -1;

   /* Remove the NPC. */
   return npc_rm( npc );
}


/**
 * @brief removes a mission NPC.
 */
int npc_rm_mission( unsigned int id, Mission *misn )
{
   NPC_t *npc;

   /* Get the NPC. */
   npc = npc_arrayGet( id );
   if (npc == NULL)
      return -1;

   /* Doesn't match type. */
   if (npc->type != NPC_TYPE_MISSION)
      return -1;

   /* Doesn't belong to the mission. */
   if (misn->id != npc->u.m.misn->id)
      return -1;

   /* Remove the NPC. */
   return npc_rm( npc );
}


/**
 * @brief NPC compare function.
 */
static int npc_compare( const void *arg1, const void *arg2 )
{
   const NPC_t *npc1, *npc2;

   npc1 = (NPC_t*)arg1;
   npc2 = (NPC_t*)arg2;

   /* Compare priority. */
   if (npc1->priority > npc2->priority)
      return +1;
   else if (npc1->priority < npc2->priority)
      return -1;
   
   return 0;
}


/**
 * @brief Sorts the NPCs.
 */
static void npc_sort (void)
{
   if (npc_array != NULL)
      qsort( npc_array, array_size(npc_array), sizeof(NPC_t), npc_compare );
}


/**
 * @brief Generates the bar missions.
 */
void npc_generate (void)
{
   int i;
   Mission *missions;
   int nmissions;

   /* Get the missions. */
   missions = missions_genList( &nmissions,
         land_planet->faction, land_planet->name, cur_system->name,
         MIS_AVAIL_BAR );

   /* Add to the bar NPC stack - may be not empty. */
   for (i=0; i<nmissions; i++)
      npc_add_giver( &missions[i] );

   /* Sort NPC. */
   npc_sort();
}


/**
 * @brief Frees a single npc.
 */
static void npc_free( NPC_t *npc )
{
   /* Common free stuff. */
   free(npc->name);
   gl_freeTexture(npc->portrait);
   free(npc->desc);

   /* Type-specific free stuff. */
   switch (npc->type) {
      case NPC_TYPE_GIVER:
         mission_cleanup(&npc->u.g);
         break;

      case NPC_TYPE_MISSION:
         free(npc->u.m.func);
         break;

      case NPC_TYPE_EVENT:
         free(npc->u.e.func);
         break;

      default:
         WARN("Freeing NPC of invalid type.");
         return;
   }
}


/**
 * @brief Cleans up the spaceport bar NPC.
 */
void npc_clear (void)
{
   int i;

   if (npc_array == NULL)
      return;

   /* First pass to clear the data. */
   for (i=0; i<array_size( npc_array ); i++) {
      npc_free( &npc_array[i] );
   }

   /* Resize down. */
   array_resize( &npc_array, 0 );
}


/**
 * @brief Frees the NPC stuff.
 */
void npc_freeAll (void)
{
   /* Clear the NPC. */
   npc_clear();

   /* Free the array. */
   if (npc_array != NULL)
      array_free( npc_array );
   npc_array = NULL;
}


/**
 * @brief Get the size of the npc array.
 */
int npc_getArraySize (void)
{
   if (npc_array == 0)
      return 0;

   return array_size( npc_array );
}


/**
 * @brief Get the npc array names for an image array.
 *
 *    @param names Name array to fill.
 *    @param n Number to fill with.
 */
int npc_getNameArray( char **names, int n )
{
   int i;

   if (npc_array == 0)
      return 0;

   /* Create the array. */
   for (i=0; i<MIN(n,array_size(npc_array)); i++)
      names[i] = strdup( npc_array[i].name );

   return i;
}


/**
 * @brief Get the npc array textures for an image array.
 *
 *    @param tex Texture array to fill.
 *    @param n Number to fill with.
 */
int npc_getTextureArray( glTexture **tex, int n )
{
   int i;

   if (npc_array == 0)
      return 0;

   /* Create the array. */
   for (i=0; i<MIN(n,array_size(npc_array)); i++)
      tex[i] = npc_array[i].portrait;

   return i;
}


/**
 * @brief Get the name of an NPC.
 */
const char *npc_getName( int i )
{
   /* Make sure in bounds. */
   if ((i<0) || (i>=array_size(npc_array)))
      return NULL;

   return npc_array[i].name;
}


/**
 * @brief Get the texture of an NPC.
 */
glTexture *npc_getTexture( int i )
{
   /* Make sure in bounds. */
   if ((i<0) || (i>=array_size(npc_array)))
      return NULL;

   return npc_array[i].portrait;
}


/**
 * @brief Gets the NPC description.
 */
const char *npc_getDesc( int i )
{
   /* Make sure in bounds. */
   if ((i<0) || (i>=array_size(npc_array)))
      return NULL;

   return npc_array[i].desc;
}


/**
 * @brief Approaches a mission giver guy.
 *
 *    @brief Returns 1 on destroyed, 0 on not destroyed.
 */
static int npc_approach_giver( NPC_t *npc )
{
   int i;
   int ret;
   Mission *misn;

   /* Make sure player can accept the mission. */
   for (i=0; i<MISSION_MAX; i++)
      if (player_missions[i].data == NULL)
         break;
   if (i >= MISSION_MAX) {
      dialogue_alert("You have too many active missions.");
      return -1;
   }

   /* Get mission. */
   misn = &npc->u.g;
   ret  = mission_accept( misn );
   if ((ret==0) || (ret==2) || (ret==-1)) { /* successs in accepting the mission */
      if (ret==-1)
         mission_cleanup( misn );
      npc_free( npc );
      array_erase( &npc_array, &npc[0], &npc[1] );
      ret = 1;
   }
   else
      ret  = 0;

   return ret;
}


/**
 * @brief Approaches the NPC.
 *
 *    @param i Index of the NPC to approach.
 */
int npc_approach( int i )
{
   NPC_t *npc;

   /* Make sure in bounds. */
   if ((i<0) || (i>=array_size(npc_array)))
      return -1;

   /* Comfortability. */
   npc = &npc_array[i];

   /* Handle type. */
   switch (npc->type) {
      case NPC_TYPE_GIVER:
         return npc_approach_giver( npc );

      case NPC_TYPE_MISSION:
         misn_run( npc->u.m.misn, npc->u.m.func );
         break;

      case NPC_TYPE_EVENT:
         event_run( npc->u.e.id, npc->u.e.func );
         break;

      default:
         WARN("Unknown NPC type!");
         return -1;
   }

   return 0;
}

