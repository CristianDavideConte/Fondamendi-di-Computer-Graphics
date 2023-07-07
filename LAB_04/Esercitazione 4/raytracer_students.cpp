#include "raytracer.h"
#include "material.h"
#include "vectors.h"
#include "argparser.h"
#include "raytree.h"
#include "utils.h"
#include "mesh.h"
#include "face.h"
#include "sphere.h"

Vec3f lerp(Vec3f a, Vec3f b, float amount) {
	return (1.0f - amount) * a + amount * b;
}

// casts a single ray through the scene geometry and finds the closest hit
bool
RayTracer::CastRay (Ray & ray, Hit & h, bool use_sphere_patches) const
{
  bool answer = false;
  Hit nearest;
  nearest = Hit();

  // intersect each of the quads
  for (int i = 0; i < mesh->numQuadFaces (); i++)
  {
	Face *f = mesh->getFace (i);
	if (f->intersect (ray, h, args->intersect_backfacing))
	{
		if( h.getT() < nearest.getT() )
		{
			answer = true;
			nearest = h;
		}
	}
  }

  // intersect each of the spheres (either the patches, or the original spheres)
  if (use_sphere_patches)
  {
	for (int i = mesh->numQuadFaces (); i < mesh->numFaces (); i++)
	{
	  Face *f = mesh->getFace (i);
	  if (f->intersect (ray, h, args->intersect_backfacing))
	  {
		if( h.getT() < nearest.getT() )
		{
			answer = true;
			nearest = h;
		}
	  }
	}
  }
  else
  {
	const vector < Sphere * >&spheres = mesh->getSpheres ();
	for (unsigned int i = 0; i < spheres.size (); i++)
	{
	  if (spheres[i]->intersect (ray, h))
	  {
		if( h.getT() < nearest.getT() )
		{
			answer = true;
			nearest = h;
		}
	  }
	}
  }

  h = nearest;
  return answer;
}

Vec3f
RayTracer::TraceRay (Ray & ray, Hit & hit, int bounce_count) const
{

  hit = Hit ();
  bool intersect = CastRay (ray, hit, false);

  if( bounce_count == args->num_bounces )
  	RayTree::SetMainSegment (ray, 0, hit.getT () );
  else
	RayTree::AddReflectedSegment(ray, 0, hit.getT() );

  Vec3f answer = args->background_color; // nelle slide si chiama "color"

  Material *m = hit.getMaterial ();
  if (intersect == true)
  {
	assert (m != NULL);
	Vec3f normal = hit.getNormal ();
	Vec3f point = ray.pointAtParameter (hit.getT ());

	// ----------------------------------------------
	// ambient light
	answer = args->ambient_light * m->getDiffuseColor ();

	// ----------------------------------------------
	// if the surface is shiny...
	Vec3f reflectiveColor = m->getReflectiveColor ();

	// ==========================================
	// ASSIGNMENT:  ADD REFLECTIVE LOGIC
	// ==========================================
	
	// se (il punto sulla superficie e' riflettente & bounce_count>0)
	if (reflectiveColor.Length() != 0 && bounce_count > 0) {
		//calcolare ReflectionRay  R=2<n,l>n -l
		Vec3f VRay = ray.getDirection();
		Vec3f reflectionRay = VRay - (2 * VRay.Dot3(normal) * normal);
		reflectionRay.Normalize();
		Ray* new_ray = new Ray(point, reflectionRay);
		
		//invocare TraceRay(ReflectionRay, hit,bounce_count-1)
		//aggiungere ad answer il contributo riflesso
		answer += TraceRay(*new_ray, hit, bounce_count - 1) * reflectiveColor;
	}

	Hit* new_hit;
	bool colpito;
	Ray* n_ray;
	Vec3f n_point, dista, pointOnLight, dirToLight;

	// ----------------------------------------------
	// add each light
	int num_lights = mesh->getLights ().size ();
	for (int i = 0; i < num_lights; i++)
	{
	  // ==========================================
	  // ASSIGNMENT:  ADD SHADOW LOGIC
	  // ==========================================
	  Face *f = mesh->getLights ()[i];

	  //computeCentroid = considero solo il baricentro della faccia/luce
	  std::vector<Vec3f> pointsOnLight{};

	  //Crea soft shadows se specificato da riga di comando.
	  //Altrimenti usa le hard shadows.
	  if (args->softShadow) {
		  /*
		  //La light area viene divisa tramite una griglia di (1 / increment_factor)^2 punti.
		  //Ogni punto della griglia viene utilizzato per il calcolo della soft shadow.
		  Vec3f v_start, v_end;
		  const float increment_factor = 0.1; //The lower the more precise the shadows will be

		  for (float j = 0; j <= 1; j+=increment_factor) {
			  //(*f)[0]->get() bottom-left face vertex
			  //(*f)[1]->get() top-left face vertex
			  //(*f)[2]->get() top-right face vertex
			  //(*f)[3]->get() bottom-right face vertex

			  v_start = lerp((*f)[0]->get(), (*f)[1]->get(), j);
			  v_end = lerp((*f)[3]->get(), (*f)[2]->get(), j);
			  
			  for (float k = 0; k <= 1; k+=increment_factor) {
				  pointsOnLight.push_back(lerp(v_start, v_end, k));
			  }
		  }
		  */

		  //Random interpolation between the face vertices (better looking)
		  for (int i = 0; i < 100; i++) {
			  pointsOnLight.push_back(f->RandomPoint());
		  }
	  } else {
		  //La luce è approssimata ad un punto solo (hard shadows)
		  pointsOnLight.push_back(f->computeCentroid()); 
	  }

	  for (int j = 0; j < pointsOnLight.size(); j++) {
		  Vec3f pointOnLight = pointsOnLight[j];
		  Vec3f dirToLight = pointOnLight - point;
		  dirToLight.Normalize ();

		  // creare shadow ray verso il punto luce
		  n_ray = new Ray(point, dirToLight);
	  
		  // controllare il primo oggetto colpito da tale raggio
		  new_hit = new Hit();
		  colpito = CastRay(*n_ray, *new_hit, false);

		  if (colpito) {
			  n_point = n_ray->pointAtParameter(new_hit->getT());
			  // calcola il vettore distanza tra il punto colpito dal raggio e il punto sulla luce
			  dista.Sub(dista, n_point, pointOnLight);

			  // Se e' la sorgente luminosa i-esima allora
			  //	calcolare e aggiungere ad answer il contributo luminoso
			  // Altrimenti
			  //    la luce i non contribuisce alla luminosita' di point.
			  if (dista.Length() < 0.01) {
				  if (normal.Dot3(dirToLight) > 0) {
					  Vec3f lightColor = 0.2 * f->getMaterial()->getEmittedColor() * f->getArea();
					  answer += m->Shade(ray, hit, dirToLight, lightColor, args);
				  }
			  }
			  else {
				  RayTree::AddShadowSegment(*n_ray, 0, new_hit->getT());
			  }
		  }
	  }

	  if (normal.Dot3 (dirToLight) > 0)
	  {
		Vec3f lightColor = 0.2 * f->getMaterial ()->getEmittedColor () * f->getArea ();
		answer += m->Shade (ray, hit, dirToLight, lightColor, args);
	  }
	}
    
  }

  return answer;
}
