#include "common.h"
#include "main/assert_fail.h"
#include "main/xgl_vector_ops.h"
#include "main/xgl_cur_matrix.h"
#include "main/xgl_2.h"
#include "main/TiDynamicsColl.h"
#include "shared.h"

/* Status threshold below which the point entries are indexed by key (else searched by key). */
#define TI_DYNAMICS_INDEXED_THRESHOLD 1.1f


/* Writes the position of the entry for key into dst with w = 1. Below a threshold of 1.1f the entries are indexed
 * directly by key; otherwise the first keyed entry with that key is used. With apply set, dst is then transformed
 * by the global matrix that the entry names. */
int tiMRPackageGetNodePoint(TiMRPackage *pMRPack, u32 key, Vector4 *dst, int apply)
{
    TiNodeTable *pStatus;
    TiNodeSlot *indexedEntries;
    TiNodeKey *keyedEntries;
    TiNodeKey *found;
    u32 entryIndex;

    pStatus = pMRPack->nodeTable;
    if (pStatus == 0) {
        return 0;
    }
    if (pStatus->threshold < TI_DYNAMICS_INDEXED_THRESHOLD) {
        indexedEntries = pStatus->slots;
        if (key >= pStatus->count) {
            return 0;
        }
        dst->x = indexedEntries[key].offset[0];
        dst->y = indexedEntries[key].offset[1];
        dst->z = indexedEntries[key].offset[2];
        dst->w = 1.0f;
        if (apply != 0) {
            xglCurMatrixSet(pMRPack->globalMat[indexedEntries[key].matrixIndex]);
            xglCurApplyVector(dst, dst);
            return 1;
        }
        return 1;
    } else {
        keyedEntries = (TiNodeKey *) pStatus->slots;
        found = 0;
        for (entryIndex = 0; entryIndex < pMRPack->nodeTable->count; entryIndex++) {
            if (keyedEntries[entryIndex].id == key) {
                found = &keyedEntries[entryIndex];
                break;
            }
        }
        if (found == 0) {
            return 0;
        }
        dst->x = found->offset[0];
        dst->y = found->offset[1];
        dst->z = found->offset[2];
        dst->w = 1.0f;
        if (apply != 0) {
            xglCurMatrixSet(pMRPack->globalMat[found->matrixIndex]);
            xglCurApplyVector(dst, dst);
        }
        return 1;
    }
}

/* Transforms each vector entry from src into dst with its global matrix. */
void tiDynamicsCollTransformVectors(TiMRPackage *pMRPack)
{
    TiCollSphere *entry;
    u32 entryIndex;

    entry = (TiCollSphere *) ((u8 *) pMRPack->hrcData
                                         + pMRPack->hrcData->sphereOffset);
    if (pMRPack->globalMat == 0) {
        assertFailed("pMRPack->pGlobalMat", "../TiDynamicsColl.euc.c", 17);
    }
    entryIndex = 0;
    if (pMRPack->hrcData->sphereCount != 0) {
        do {
            /* entry advances only for the entries that are transformed (original shape). */
            if (entry->matrixIndex != -1) {
                xglVectorMulMat(&entry->center, pMRPack->globalMat[entry->matrixIndex], &entry->src);
                entry++;
            }
            entryIndex++;
        } while (entryIndex < pMRPack->hrcData->sphereCount);
    }
}

/* Fills each matrix entry from its global matrix; index -1 runs the VU0 store
 * path (tiVu0LoadIdentityMatrix, xglCurMatrixGet) instead. */
void tiDynamicsCollBuildInverseMatrices(TiMRPackage *pMRPack)
{
    TiHrcData *hrc;
    TiCollMatrix *entry;
    u32 entryIndex;

    entryIndex = 0;
    hrc = pMRPack->hrcData;
    entry = (TiCollMatrix *) ((u8 *) hrc + hrc->matrixOffset);
    if (hrc->matrixCount != 0) {
        do {
            if (entry->index == -1) {
                tiVu0LoadIdentityMatrix();
                xglCurMatrixGet(entry->matrix_90.rows);
            } else {
                xglMatrixInverse(&entry->matrix_90, (Matrix *) (pMRPack->globalMat + entry->index));
            }
            entryIndex++;
            entry++;
        } while (entryIndex < pMRPack->hrcData->matrixCount);
    }
}

/* Same as the matrix entry pass, for the wide entry list. */
void tiDynamicsCollBuildWideInverseMatrices(TiMRPackage *pMRPack)
{
    TiHrcData *hrc;
    TiCollCapsule *entry;
    u32 entryIndex;

    entryIndex = 0;
    hrc = pMRPack->hrcData;
    entry = (TiCollCapsule *) ((u8 *) hrc + hrc->capsuleOffset);
    if (hrc->capsuleCount != 0) {
        do {
            if (entry->index == -1) {
                tiVu0LoadIdentityMatrix();
                xglCurMatrixGet(entry->matrix_a0.rows);
            } else {
                xglMatrixInverse(&entry->matrix_a0, (Matrix *) (pMRPack->globalMat + entry->index));
            }
            entryIndex++;
            entry++;
        } while (entryIndex < pMRPack->hrcData->capsuleCount);
    }
}
