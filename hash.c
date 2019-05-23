#define _POSIX_C_SOURCE 200809L
#include "hash.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAM_INI 37
#define FACTOR 271.82818285//aproximación al número e

/* ------- Enumerate de los posibles estados de los campos ----- */
typedef enum estado { VACIO, OCUPADO, BORRADO } estado_t;

/* --------------------- Struct de los campos ------------------ */
typedef struct hash_campo{
	char* clave;
	void* valor;
	estado_t estado;
} hash_campo_t;

/* ------------------------ Struct del Hash -------------------- */
typedef struct hash{
	hash_destruir_dato_t destruir_dato;
	hash_campo_t* tabla;
	size_t cantidad;
	size_t capacidad;
}hash;


/* ------------------- Funciones Auxiliares ------------------ */

// función de hashing - sdbm
size_t f_hash(const char *str){
    size_t hash = 5381;
    int c = 0;
    while ((c = *str++)){
        hash =  c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

hash_campo_t* buscar(const hash_t* hash, const char* clave){
	size_t j = 0;
	size_t pos_aux = f_hash(clave);
	while(j != hash->capacidad){
		size_t pos = (pos_aux + j*j) % hash->capacidad;
        if (hash->tabla[pos].estado == VACIO){
        	return NULL;
        }
        if (hash->tabla[pos].estado == OCUPADO && strcmp(hash->tabla[pos].clave, clave) == 0){
			return &hash->tabla[pos];
		}     
	    j++;
	}
	return NULL;
}

//size_t prox_primo(size_t capacidad);

bool redimensionar(hash_t* hash){
	
	hash_campo_t* tabla_aux = calloc(hash->capacidad * 2, sizeof(hash_campo_t));
	if(!tabla_aux){
		return false;
	}
	for(int i = 0; i < hash->capacidad; i++){
		if(hash->tabla[i].estado == OCUPADO){
			size_t j = 0;
			size_t pos_aux = f_hash(hash->tabla[i].clave);
			while(j != hash->capacidad * 2){
				size_t pos = (pos_aux + j*j) % (hash->capacidad * 2);
				if(tabla_aux[pos].estado != OCUPADO){
					tabla_aux[pos].clave = hash->tabla[i].clave;
					tabla_aux[pos].valor = hash->tabla[i].valor;
					tabla_aux[pos].estado = OCUPADO;
					break;
				}
				j++;
			}
		}
		if(hash->tabla[i].estado == BORRADO){
			free(hash->tabla[i].clave);
			if(hash->destruir_dato){
				hash->destruir_dato(hash->tabla[i].valor);
			}
		}
	}
	free(hash->tabla);
	hash->tabla = tabla_aux;
	hash->capacidad = hash->capacidad * 2; 
	return true;
}

/* -------------------- Primitivas del Hash ------------------- */

hash_t* hash_crear(hash_destruir_dato_t destruir_dato){
	hash_t* hash = malloc(sizeof(hash_t));
	if(!hash){
		return NULL;
	}
	hash_campo_t* tabla = calloc(TAM_INI, sizeof(hash_campo_t));
	if(!tabla){
		free(hash);
		return NULL;
	}
	hash->cantidad = 0;
	hash->capacidad = TAM_INI;
	hash->destruir_dato = destruir_dato;
	hash->tabla = tabla;
	return hash;
}

bool hash_guardar(hash_t *hash, const char *clave, void *dato){
	if((hash->cantidad * 1000)/hash->capacidad >= FACTOR && !redimensionar(hash)){
		return false;
		}
	size_t j = 0;
	size_t pos_func = f_hash(clave);
	while(j != hash->capacidad){
		size_t pos = (pos_func + j*j) % hash->capacidad;
		if(hash->tabla[pos].estado != OCUPADO){
			char* clave_cpy = strdup(clave);
			if(!clave_cpy){
				return false;
			}
			if(hash->tabla[pos].estado == BORRADO){
				free(hash->tabla[pos].clave);
				if(hash->destruir_dato){
					hash->destruir_dato(hash->tabla[pos].valor);
				}
			}
			hash->tabla[pos].clave = clave_cpy;
			hash->tabla[pos].valor = dato;
			hash->tabla[pos].estado = OCUPADO;
			hash->cantidad++;
			return true;
		}
		if(strcmp(hash->tabla[pos].clave, clave) == 0){
			if(hash->destruir_dato){
				hash->destruir_dato(hash->tabla[pos].valor);
			}
			hash->tabla[pos].valor = dato;
			return true;
		}
		j++;
	}
	return false;
}

void *hash_borrar(hash_t *hash, const char *clave){
	hash_campo_t* campo = buscar(hash, clave);
	if(!campo){
		return NULL;
	}
	campo->estado = BORRADO;
	hash->cantidad--;
	return campo->valor;
}

void *hash_obtener(const hash_t* hash, const char *clave){
	hash_campo_t* campo = buscar(hash, clave);
	if(!campo){
		return NULL;
	}
	return campo->valor;
}

bool hash_pertenece(const hash_t *hash, const char *clave){
	return buscar(hash, clave) != NULL;
}

size_t hash_cantidad(const hash_t *hash){
	return hash->cantidad;
}

void hash_destruir(hash_t *hash){
	for(size_t i=0; i < hash->capacidad; i++){
		if(hash->tabla[i].estado != VACIO){
			free(hash->tabla[i].clave);
			if(hash->destruir_dato){
				hash->destruir_dato(hash->tabla[i].valor);
			}
		}
	}
	free(hash->tabla);
	free(hash);
}


/* --------------- Struct del iterador del Hash --------------- */
typedef struct hash_iter{
	const hash_t* hash;
	size_t pos;
}hash_iter;

/* --------------- funciones privadas --------------- */

size_t siguiente_posicion_activa(const hash_t* hash, size_t pos){
	if(!hash)return 0;

	while(pos < hash->capacidad && hash->tabla[pos].estado != OCUPADO){
		pos++;
	}
	return pos;
}

/* ------------------ Primitivas del Iterador ----------------- */

hash_iter_t *hash_iter_crear(const hash_t *hash){
	hash_iter_t* iter = malloc(sizeof(hash_iter_t));
	if(!iter)return NULL;
	iter->hash = hash;
	iter->pos = siguiente_posicion_activa(iter->hash,0);
	return iter;
}

bool hash_iter_avanzar(hash_iter_t *iter){
	if(iter->hash->cantidad == 0 || hash_iter_al_final(iter))return false;
	iter->pos = siguiente_posicion_activa(iter->hash, iter->pos + 1);
	return true;
}

const char *hash_iter_ver_actual(const hash_iter_t *iter){
	if(iter->hash->cantidad == 0 || iter->hash->capacidad == iter->pos)return NULL;
	return iter->hash->tabla[iter->pos].clave;
}

bool hash_iter_al_final(const hash_iter_t *iter){
	if(!iter->hash)return true;
	return iter->pos == iter->hash->capacidad;
}

void hash_iter_destruir(hash_iter_t* iter){
	free(iter);
}