#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PBFT_R_Abort_certificate.h"
#include "Array.h"
#include "parameters.h"

PBFT_R_Abort_certificate::PBFT_R_Abort_certificate(int comp, int num_rep) :
   ac()
{
   //fprintf(stderr, "PBFT_R_Abort certificate initialized for %d replicas\n", num_rep);
   ac.append(NULL, num_rep);
   count = 0;
   complete = comp;
}

PBFT_R_Abort_certificate::~PBFT_R_Abort_certificate()
{
}

bool PBFT_R_Abort_certificate::add(PBFT_R_Abort *abort)
{
   const int id = abort->id();

   if (!ac[id])
   {
      Ac_entry *ace = new Ac_entry(abort);
      ac[id] = ace;
      count++;
      return true;
   } else if (ac[id]->abort->id() == abort->id()
	   && ac[id]->abort->digest() == abort->digest()
	   && ac[id]->abort->request_id() == abort->request_id())
   {
       return true;
   }
   return false;
}

void PBFT_R_Abort_certificate::clear()
{
   for (int i = 0; i<ac.size() ; i++)
   {
      delete ac[i];
      ac[i] = NULL;
   }

   count = 0;
}
