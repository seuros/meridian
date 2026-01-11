/* $Id: fsw_core.c 29125 2010-05-06 09:43:05Z vboxsync $ */
/** @file
 * fsw_core.c - Core File System Wrapper Abstraction Layer.
**/

/**
 * This code is based on:
 *
 * Copyright (c) 2006 Christoph Pfisterer
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/
/**
** Modified for RefindPlus
** Copyright (c) 2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the MIT License.
**/

#include "fsw_core.h"
#include "fsw_efi.h"


// functions

static void fsw_blockcache_free(struct fsw_volume *vol);

#define MAX_CACHE_LEVEL (5)

/**
 * Mount a volume with a given filesystem driver. This function is called by
 * the host driver to make a volume accessible. The filesystem driver to use
 * is specified by a pointer to its dispatch table. The filesystem driver will
 * look at the data on the volume to determine if it can read the format.
 * If the volume is found unsuitable, FSW_UNSUPPORTED is returned.
 *
 * If this function returns FSW_SUCCESS, *vol_out points at a valid volume data
 * structure. The caller must release it later by calling fsw_unmount.
 *
 * If this function returns an error status, the caller only needs to clean up
 * its own buffers that may have been allocated through the read_block interface.
**/

fsw_status_t fsw_mount (
    void                     *host_data,
    struct fsw_host_table    *host_table,
    struct fsw_fstype_table  *fstype_table,
    struct fsw_volume       **vol_out
) {
    fsw_status_t    status;
    struct fsw_volume *vol;


    // Allocate memory for the structure
    status = fsw_alloc_zero (
        fstype_table->volume_struct_size,
        (void **) &vol
    );
    if (status) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_mount ... Leaving with Status '%d' Error (1):- Memory Allocation Failure\n"
            ), status
        ));

        return status;
    }

    // Initialize fields
    vol->phys_blocksize   = 512;
    vol->log_blocksize    = 512;
    vol->label.type       = FSW_STRING_TYPE_EMPTY;
    vol->host_data        = host_data;
    vol->host_table       = host_table;
    vol->fstype_table     = fstype_table;
    vol->host_string_type = host_table->native_string_type;

    // Let the driver mount the filesystem
    status = vol->fstype_table->volume_mount (vol);
    if (status) goto errorexit;

    *vol_out = vol;
    return FSW_SUCCESS;

errorexit:
    fsw_unmount (vol);

    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_CORE: fsw_mount ... Leaving with Status '%d' Error (2)\n"
        ), status
    ));

    return status;
}

/**
 * Unmount a volume by releasing all memory associated with it. This function
 * is called by the host driver when a volume is no longer needed. It is also
 * called by the core after a failed mount to clean up any allocated memory.
 *
 * Note that all dnodes must have been released before calling this function.
**/

void fsw_unmount (
    struct fsw_volume *vol
) {
    if (vol->root) fsw_dnode_release (vol->root);
    // TODO: check that no other dnodes are still around

    vol->fstype_table->volume_free (vol);

    fsw_blockcache_free (vol);
    fsw_strfree (&vol->label);
    FSW_DO_FREE(vol);
}

/**
 * Get in-depth information on the volume. This function can be called
 * by the host driver to get additional information on the volume.
**/

fsw_status_t fsw_volume_stat (
    struct fsw_volume      *vol,
    struct fsw_volume_stat *sb
) {
    return vol->fstype_table->volume_stat (vol, sb);
}

/**
 * Set the physical and logical block sizes of the volume. This functions is
 * called by the filesystem driver to announce the block sizes it wants to use
 * for accessing the disk (physical) and for addressing file contents (logical).
 * Usually both sizes will be the same but there may be filesystems that need
 *to access  metadata at a smaller block size than the allocation unit for files.
 *
 * Calling this function causes the block cache to be dropped. All pointers
 * returned from fsw_block_get become invalid. This function should only be
 * called while mounting the filesystem, not as a part of file access operations.
 *
 * Both sizes are measured in bytes, must be powers of 2, and at least 512 bytes.
 * The logical block size cannot be smaller than the physical block size.
**/

void fsw_set_blocksize (
    struct fsw_volume *vol,
    fsw_u32            phys_blocksize,
    fsw_u32            log_blocksize
) {
    // TODO: Check the sizes. Both must be powers of 2.
    //       log_blocksize must not be smaller than phys_blocksize.

    // drop core block cache if present
    fsw_blockcache_free (vol);

    // signal host driver to drop caches etc.
    vol->host_table->change_blocksize (
        vol,
        vol->phys_blocksize, vol->log_blocksize,
        phys_blocksize, log_blocksize
    );

    vol->phys_blocksize = phys_blocksize;
    vol->log_blocksize  = log_blocksize;
}

/**
 * Get a block of data from the disk. This function is called by the filesystem
 * driver or by core functions. It calls through to the host driver's device
 * access routine. Given a physical block number, it reads the block into memory
 * (or fetches it from the block cache) and returns the address of the memory
 * buffer. The caller should provide an indication of how important the block
 * is in the cache_level parameter. Blocks with a low level are purged first.
 *
 * Some suggestions for cache levels:
 *   - 0: File data
 *   - 1: Directory data, symlink data
 *   - 2: Filesystem metadata
 *   - 3..5: Filesystem metadata with a high rate of access
 *
 * If this function returns successfully, the returned data pointer is valid
 * until the caller calls fsw_block_release.
**/

fsw_status_t fsw_block_get (
    struct VOLSTRUCTNAME  *vol,
    fsw_u64                phys_bno,
    fsw_u32                cache_level,
    void                 **buffer_out
) {
    fsw_status_t           status;
    fsw_u32                i, discard_level, new_bcache_size;
    struct fsw_blockcache *new_bcache;

    // TODO: Allow the host driver to do its own caching;
    //       Call through if appropriate function pointers are set

    if (cache_level > MAX_CACHE_LEVEL) {
        cache_level = MAX_CACHE_LEVEL;
    }

    // Check block cache
    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].phys_bno == phys_bno) {
            // Cache Hit
            if (vol->bcache[i].cache_level < cache_level) {
                vol->bcache[i].cache_level = cache_level;  // Promote entry
            }

            vol->bcache[i].refcount++;
            *buffer_out = vol->bcache[i].data;

            return FSW_SUCCESS;
        }
    }

    // Find a free entry in the cache table
    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].phys_bno == (fsw_u64) FSW_INVALID_BNO) {
            break;
        }
    }
    if (i >= vol->bcache_size) {
        for (
            discard_level = 0;
            discard_level <= MAX_CACHE_LEVEL;
            discard_level++
        ) {
            for (i = 0; i < vol->bcache_size; i++) {
                if (vol->bcache[i].refcount    == 0 &&
                    vol->bcache[i].cache_level <= discard_level
                ) {
                    break;
                }
            }
            if (i < vol->bcache_size) break;
        }
    }
    if (i >= vol->bcache_size) {
        // Enlarge/Create Cache
        new_bcache_size = (
            vol->bcache_size < 16
        ) ? 16 : vol->bcache_size << 1;

        status = FSW_DO_ALLOC(
            new_bcache_size * sizeof (struct fsw_blockcache),
            &new_bcache
        );
        if (status) {
            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_block_get ... Leaving with Status '%d' Error (Tag_01)\n"
                ), status
            ));

            return status;
        }

        if (vol->bcache_size > 0) {
            FSW_DO_MEMCPY(
                new_bcache, vol->bcache,
                vol->bcache_size * sizeof (struct fsw_blockcache)
            );
        }

        for (i = vol->bcache_size; i < new_bcache_size; i++) {
            new_bcache[i].refcount = 0;
            new_bcache[i].cache_level = 0;
            new_bcache[i].phys_bno = (fsw_u64) FSW_INVALID_BNO;
            new_bcache[i].data = NULL;
        }
        i = vol->bcache_size;

        // Switch caches
        if (vol->bcache != NULL) {
            FSW_DO_FREE(vol->bcache);
        }
        vol->bcache = new_bcache;
        vol->bcache_size = new_bcache_size;
    }
    vol->bcache[i].phys_bno = (fsw_u64) FSW_INVALID_BNO;

    // Read the data
    if (vol->bcache[i].data == NULL) {
        status = FSW_DO_ALLOC(
            vol->phys_blocksize,
            &vol->bcache[i].data
        );
        if (status) {
            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_block_get ... Leaving with Status '%d' Error (Tag_02)\n"
                ), status
            ));

            return status;
        }
    }

    status = vol->host_table->read_block (
        vol, phys_bno,
        vol->bcache[i].data
    );
    if (status) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_block_get ... Leaving with Status '%d' Error (Tag_03)\n"
            ), status
        ));

        return status;
    }

    vol->bcache[i].phys_bno = phys_bno;
    vol->bcache[i].cache_level = cache_level;
    vol->bcache[i].refcount = 1;

    *buffer_out = vol->bcache[i].data;
    return FSW_SUCCESS;
}

/**
 * Releases a disk block. This function must be called
 * to release disk blocks returned from fsw_block_get.
**/

void fsw_block_release (
    struct VOLSTRUCTNAME *vol,
    fsw_u64               phys_bno,
    void                 *buffer
) {
    fsw_u32 i;


    // TODO: Allow host driver to do its own caching;
    //       Just call through if appropriate function pointers are set

    // update block cache
    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].refcount >  0 &&
            vol->bcache[i].phys_bno == phys_bno
        ) {
            vol->bcache[i].refcount--;
        }
    }
}

/**
 * Release the block cache. Called internally when changing block sizes and when
 * unmounting the volume. It frees all data occupied by the generic block cache.
**/

static void fsw_blockcache_free (
    struct fsw_volume *vol
) {
    fsw_u32 i;


    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].data != NULL) {
            FSW_DO_FREE(vol->bcache[i].data);
        }
    }
    if (vol->bcache != NULL) {
        FSW_DO_FREE(vol->bcache);
        vol->bcache = NULL;
    }
    vol->bcache_size = 0;
    fsw_efi_clear_cache();
}

/**
 * Add a new dnode to the list of known dnodes. This internal function is used
 * when a dnode is created to add it to the dnode list that is used to search
 * for existing dnodes by id.
**/

static void fsw_dnode_register (
    struct fsw_volume *vol,
    struct fsw_dnode  *dno
) {
    dno->next = vol->dnode_head;
    if (vol->dnode_head != NULL) {
        vol->dnode_head->prev = dno;
    }
    dno->prev = NULL;
    vol->dnode_head = dno;
}

/**
 * Create a dnode representing the root directory. This function is called
 * by the filesystem driver while mounting the filesystem. The root directory
 * is special because it has no parent dnode, its name is defined to be empty,
 * and its type is fixed. Otherwise, this functions behaves as fsw_dnode_create.
**/

fsw_status_t fsw_dnode_create_root_with_tree (
    struct fsw_volume *vol,
    fsw_u64            tree_id,
    fsw_u64            dnode_id,
    struct fsw_dnode **dno_out
) {
    fsw_status_t    status;
    struct fsw_dnode *dno;


    // Allocate memory for the structure
    status = fsw_alloc_zero (
        vol->fstype_table->dnode_struct_size,
        (void **) &dno
    );
    if (status) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_create_root_with_tree ... Leaving with Status '%d' Error (1)\n"
            ), status
        ));

        return status;
    }

    // Fill the structure
    dno->vol = vol;
    dno->parent = NULL;
    dno->tree_id = tree_id;
    dno->dnode_id = dnode_id;
    dno->refcount = 1;
    dno->type = FSW_DNODE_TYPE_DIR;
    dno->name.type = FSW_STRING_TYPE_EMPTY;
    // TODO: Call a func to create an empty string in the native string type instead

    fsw_dnode_register(vol, dno);

    *dno_out = dno;
    return FSW_SUCCESS;
}

fsw_status_t fsw_dnode_create_root (
    struct fsw_volume *vol,
    fsw_u64            dnode_id,
    struct fsw_dnode **dno_out
) {
	return fsw_dnode_create_root_with_tree (
        vol, 0, dnode_id, dno_out
    );
}
/**
 * Create a new dnode representing a filesystem object. This function is called
 * by the filesystem driver in response to directory lookup or read requests.
 * Note that if there already is a dnode with the given dnode_id on record,
 * then no new object is created. Instead, the existing dnode is returned and
 * its reference count increased. All other parameters are ignored in this case.
 *
 * The type passed into this function may be FSW_DNODE_TYPE_UNKNOWN.
 * It is sufficient to fill the type field during the dnode_fill call.
 *
 * The name parameter must describe a string with the object's name. A copy will
 * be stored in the dnode structure for future reference. The name will not be
 * used to shortcut directory lookups, but may be used to reconstruct paths.
 *
 * If the function returns successfully, *dno_out contains a pointer to the
 * dnode that must be released by the caller with fsw_dnode_release.
**/

fsw_status_t fsw_dnode_create_with_tree (
    struct fsw_dnode   *parent_dno,
    fsw_u64             tree_id,
    fsw_u64             dnode_id,
    int                 type,
    struct fsw_string  *name,
    struct fsw_dnode  **dno_out
) {
    fsw_status_t       status;
    struct fsw_volume *vol = parent_dno->vol;
    struct fsw_dnode  *dno;

    // Check if we already have a dnode with the same id
    for (dno = vol->dnode_head; dno; dno = dno->next) {
        if (dno->dnode_id == dnode_id && dno->tree_id == tree_id) {
            fsw_dnode_retain (dno);

            *dno_out = dno;
            return FSW_SUCCESS;
        }
    }

    // Allocate memory for the structure
    status = fsw_alloc_zero (
        vol->fstype_table->dnode_struct_size,
        (void **) &dno
    );
    if (status) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_create_with_tree ... Leaving with Status '%d' Error (1)\n"
            ), status
        ));

        return status;
    }

    // Fill the structure
    dno->vol = vol;
    dno->parent = parent_dno;
    fsw_dnode_retain (dno->parent);
    dno->tree_id = tree_id;
    dno->dnode_id = dnode_id;
    dno->type = type;
    dno->refcount = 1;
    status = fsw_strdup_coerce (
        &dno->name,
        vol->host_table->native_string_type,
        name
    );
    if (status) {
        FSW_DO_FREE(dno);

        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_create_with_tree ... Leaving with Status '%d' Error (2)\n"
            ), status
        ));

        return status;
    }

    fsw_dnode_register (vol, dno);

    *dno_out = dno;
    return FSW_SUCCESS;
}

fsw_status_t fsw_dnode_create (
    struct fsw_dnode   *parent_dno,
    fsw_u64             dnode_id,
    int                 type,
    struct fsw_string  *name,
    struct fsw_dnode  **dno_out
) {
	return fsw_dnode_create_with_tree (
        parent_dno, 0, dnode_id,
        type, name, dno_out
    );
}

/**
 * Increases the reference count of a dnode. This must be balanced with
 * fsw_dnode_release calls. Note that some dnode functions return
 * a retained dnode pointer to their caller.
**/

void fsw_dnode_retain (
    struct fsw_dnode *dno
) {
    dno->refcount++;
}

/**
 * Release a dnode pointer, deallocating it if this was the last reference.
 * This function decrements the reference counter of the dnode. If the counter
 * reaches zero, the dnode is freed. Since the parent dnode is released
 * during that process, this function may cause it to be freed, too.
**/

void fsw_dnode_release (
    struct fsw_dnode *dno
) {
    struct fsw_volume *vol = dno->vol;
    struct fsw_dnode *parent_dno;

    dno->refcount--;

    if (dno->refcount == 0) {
        parent_dno = dno->parent;

        // De-register from volume's list
        if (dno->next)              dno->next->prev = dno->prev;
        if (dno->prev)              dno->prev->next = dno->next;
        if (vol->dnode_head == dno) vol->dnode_head = dno->next;

        // Run fstype-specific cleanup
        vol->fstype_table->dnode_free (vol, dno);

        fsw_strfree (&dno->name);
        FSW_DO_FREE(dno);

        // Release pointer to the parent, possibly deallocating it, too
        if (parent_dno) fsw_dnode_release (parent_dno);
    }
}

/**
 * Get full information about a dnode from disk. This function is called by the
 * host driver as well as by the core functions. Some filesystems defer reading
 * full information on a dnode until it is actually needed (separation between
 * directory and inode information). This function makes sure that all
 * information is available in the dnode structure. The following fields
 * may not have correct values until fsw_dnode_fill has been called: type, size
**/

fsw_status_t fsw_dnode_fill (
    struct fsw_dnode *dno
) {
    // TODO: Check a flag right here
    //       Call fstype's dnode_fill only once per dnode

    return dno->vol->fstype_table->dnode_fill (dno->vol, dno);
}

/**
 * Get extended information about a dnode. This function can be called by the
 * host driver to get a full compliment of information about a dnode in
 * addition to the fields of the fsw_dnode structure itself.
 *
 * Some data requires host-specific conversion to be useful (i.e. timestamps)
 * and will be passed to callback functions instead of being written into
 * the structure. These callbacks must be filled in by the caller.
**/

fsw_status_t fsw_dnode_stat(
    struct fsw_dnode      *dno,
    struct fsw_dnode_stat *sb
) {
    fsw_status_t    status;


    status = fsw_dnode_fill (dno);
    if (status) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_stat ... Exit on Failure to Load Node Info\n"
            )
        ));

        return status;
    }

    sb->used_bytes = 0;
    status = dno->vol->fstype_table->dnode_stat (
        dno->vol, dno, sb
    );
    if (!status && !sb->used_bytes) {
        sb->used_bytes = FSW_U64_DIV(
            dno->size + dno->vol->log_blocksize - 1,
            dno->vol->log_blocksize
        );
    }

    return status;
}

/**
 * Lookup a directory entry by name. This function is called by the host driver.
 * Given a directory dnode and a file name, it looks up the named entry in the
 * directory.
 *
 * If the dnode is not a directory, the call will fail. The caller is responsible
 * for resolving symbolic links before calling this function.
 *
 * If the function returns FSW_SUCCESS, *child_dno_out points to the requested
 * directory entry. The caller must call fsw_dnode_release on it.
**/

fsw_status_t fsw_dnode_lookup (
    struct fsw_dnode   *dno,
    struct fsw_string  *lookup_name,
    struct fsw_dnode  **child_dno_out
) {
    fsw_status_t    status;


    status = fsw_dnode_fill (dno);
    if (status) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup ... Exit on Failure to Load Node Info\n"
            ), status
        ));

        return status;
    }

    if (dno->type != FSW_DNODE_TYPE_DIR) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup ... Leaving with Status: FSW_UNSUPPORTED\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    return dno->vol->fstype_table->dir_lookup (
        dno->vol, dno,
        lookup_name,
        child_dno_out
    );
}

/**
 * Find a filesystem object by path. This function is called by the host driver.
 * Given a directory dnode and a relative or absolute path, it walks the directory
 * tree until it finds the target dnode. Intermediate symlink nodes are resolved
 * automatically. The target dnode is not resolved if it is a symlink.
 *
 * If the function returns FSW_SUCCESS, *child_dno_out points to the requested
 * directory entry. The caller must call fsw_dnode_release on it.
**/

fsw_status_t fsw_dnode_lookup_path (
    struct fsw_dnode *dno,
    struct fsw_string *lookup_path,
    char separator,
    struct fsw_dnode **child_dno_out
) {
    fsw_status_t    status;
    struct fsw_volume *vol = dno->vol;
    struct fsw_dnode *child_dno = NULL;
    struct fsw_string lookup_name;
    struct fsw_string remaining_path;
    int             root_if_empty;

    remaining_path = *lookup_path;
    fsw_dnode_retain (dno);

    // Loop over the path
    root_if_empty = 1;
    while (1) {
        // Parse next path component
        fsw_strsplit (
            &lookup_name, &remaining_path, separator
        );

        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Split Path into '%s' and '%s'\n"
            ), lookup_name.data, remaining_path.data
        ));

        if (fsw_strlen (&lookup_name) == 0) {
            // Empty path component
            child_dno = (
                root_if_empty
            ) ? vol->root : dno;
            fsw_dnode_retain (child_dno);
        }
        else {
            // Load dno data
            status = fsw_dnode_fill (dno);
            if (status) {
                FSW_MSG_LEVEL_3((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_dnode_lookup_path ... Exit on Failure to Load Node Info (1)\n"
                    )
                ));

                goto errorexit;
            }

            // Resolve symlink (if needed)
            if (dno->type == FSW_DNODE_TYPE_SYMLINK) {
                status = fsw_dnode_resolve (dno, &child_dno);
                if (status) {
                    FSW_MSG_LEVEL_3((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup_path ... Exit on Failure to Resolve Symlink\n"
                        )
                    ));

                    goto errorexit;
                }

                // Retain symlink target as new dno
                fsw_dnode_release (dno);
                dno = child_dno;   // Already retained
                child_dno = NULL;

                // Load dno data
                status = fsw_dnode_fill (dno);
                if (status) {
                    FSW_MSG_LEVEL_3((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup ... Exit on Failure to Load Node Info (2)\n"
                        )
                    ));

                    goto errorexit;
                }
            }

            // Ensure operating on a directory
            if (dno->type != FSW_DNODE_TYPE_DIR) {
                status = FSW_UNSUPPORTED;

                FSW_MSG_LEVEL_3((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_dnode_lookup_path ... Exit with Directory Error 'FSW_UNSUPPORTED'\n"
                    )
                ));

                goto errorexit;
            }

            // Check special paths
            if (fsw_streq_cstr (&lookup_name, ".")) {
                // Self directory
                child_dno = dno;
                fsw_dnode_retain (child_dno);

                FSW_MSG_LEVEL_3((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_dnode_lookup_path ... Handling Special Case ( . )\n"
                    )
                ));
            }
            else if (
                fsw_streq_cstr (&lookup_name, "..")
            ) {
                // Parent directory
                if (dno->parent == NULL) {
                    // We cannot go up from the root directory.
                    // Caution: Apps like the uEFI shell rely on this behaviour!
                    status = FSW_NOT_FOUND;

                    FSW_MSG_LEVEL_3((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup_path ... Handling Special Case ( .. )\n"
                        )
                    ));

                    goto errorexit;
                }

                child_dno = dno->parent;
                fsw_dnode_retain (child_dno);
            }
            else {
                // Do an actual lookup
                status = vol->fstype_table->dir_lookup (
                    vol, dno,
                    &lookup_name, &child_dno
                );
                if (status) {
                    FSW_MSG_LEVEL_3((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup_path ... Exit on Failed Actual Lookup with Error '%d'\n"
                        ), status
                    ));

                    goto errorexit;
                }
            }
        }
        if (root_if_empty) root_if_empty = 0;

        // child_dno becomes new dno
        fsw_dnode_release (dno);
        dno = child_dno;   // Already retained
        child_dno = NULL;

        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Current Inode ID is %d\n"
            ), dno->dnode_id
        ));

        if (remaining_path.len < 1) break;
    } // for

    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_CORE: fsw_dnode_lookup_path ... Leaving with Status: FSW_SUCCESS\n"
        )
    ));

    *child_dno_out = dno;
    return FSW_SUCCESS;

errorexit:
    if (status == FSW_NOT_FOUND) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Leaving with Status: FSW_NOT_FOUND\n"
            )
        ));
    }
    else {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Leaving with Status '%d' Error\n"
            ), status
        ));
    }

    fsw_dnode_release (dno);
    if (child_dno != NULL) {
        fsw_dnode_release (child_dno);
    }

    return status;
} // fsw_status_t fsw_dnode_lookup_path()

/**
 * Get the next directory item in sequential order.
 * This function is called by the host driver to read the complete contents
 * of a directory in sequential (filesystem defined) order.
 * Calling this function returns the next entry. Iteration state is kept by
 * a shandle on the directory's dnode. The caller must set up the shandle
 * when starting the iteration.
 *
 * When the end of the directory is reached, the function returns FSW_NOT_FOUND.
 * If the function returns FSW_SUCCESS, *child_dno_out points to the next
 * directory entry. The caller must call fsw_dnode_release on it.
**/

fsw_status_t fsw_dnode_dir_read (
    struct fsw_shandle  *shand,
    struct fsw_dnode   **child_dno_out
) {
    fsw_status_t      status;
    fsw_u64           saved_pos;
    struct fsw_dnode *dno = shand->dnode;

    if (dno->type != FSW_DNODE_TYPE_DIR) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status: FSW_UNSUPPORTED\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    saved_pos = shand->pos;
    status = dno->vol->fstype_table->dir_read (
        dno->vol, dno, shand, child_dno_out
    );
    if (status == FSW_SUCCESS) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status: FSW_SUCCESS\n"
            )
        ));
    }
    else {
        shand->pos = saved_pos;

        if (status == FSW_NOT_FOUND) {
            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status: FSW_NOT_FOUND\n"
                )
            ));
        }
        else {
            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status '%d' Error\n"
                ), status
            ));
        }
    }

    return status;
}

/**
 * Read the target path of a symbolic link.
 * This function is called by the host driver to read the "content" of
 * a symbolic link, that is the relative or absolute path it points to.
 *
 * If the function returns FSW_SUCCESS, the string handle provided by the
 * caller is filled with a string in the host's preferred encoding.
 * The caller is responsible for calling fsw_strfree on the string.
**/

fsw_status_t fsw_dnode_readlink (
    struct fsw_dnode  *dno,
    struct fsw_string *target_name
) {
    fsw_status_t    status;


    status = fsw_dnode_fill (dno);
    if (status) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_readlink ... Leaving with Status '%d' Error\n"
            ), status
        ));

        return status;
    }

    if (dno->type != FSW_DNODE_TYPE_SYMLINK) {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_readlink ... Leaving with Status: FSW_UNSUPPORTED (Not Symlink)\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    // CWE-20  [False Positive: Improper Input Validation]
    //         'fsw_string' used intentionally for string-safe
    //         operations. Type is struct and not raw C string
    // CWE-362 [False Positive: TOCTOU Race Condition]
    //         Executing in single-threaded UEFI context
    //         Concurrent access not possible
    /* Flawfinder: ignore */
    return dno->vol->fstype_table->readlink (
        dno->vol, dno, target_name
    );
}

/**
 * Read the target path of a symbolic link by accessing file data. This function
 * can be called by the filesystem driver if the filesystem stores the target
 * path as normal file data. This function will open an shandle, read the whole
 * content of the file into a buffer, and build a string from that. Currently
 * the encoding for the string is fixed as FSW_STRING_TYPE_ISO88591.
 *
 * If the function returns FSW_SUCCESS, the string handle provided by the caller
 * is filled with a string in the host's preferred encoding. The caller is
 * responsible for calling fsw_strfree on the string.
**/

fsw_status_t fsw_dnode_readlink_data (
    struct fsw_dnode *dno,
    struct fsw_string *link_target
) {
    fsw_status_t       status;
    struct fsw_shandle shand;
    fsw_u32            buffer_size;
    char               buffer[FSW_PATH_MAX];

    struct fsw_string s;

    if (dno->size > FSW_PATH_MAX) {
        return FSW_VOLUME_CORRUPTED;
    }

    s.type = FSW_STRING_TYPE_ISO88591;
    s.size = s.len = (int)dno->size;
    s.data = buffer;

    // Open shandle and read the data
    status = fsw_shandle_open (dno, &shand);
    if (status) return status;

    buffer_size = (fsw_u32)s.size;
    status = fsw_shandle_read (
        &shand, &buffer_size, buffer
    );

    fsw_shandle_close(&shand);
    if (status) return status;

    if ((int)buffer_size < s.size) {
        return FSW_VOLUME_CORRUPTED;
    }

    status = fsw_strdup_coerce (
        link_target,
        dno->vol->host_string_type, &s
    );

    return status;
}

/**
 * Resolve a symbolic link. This function can be called by the host driver to
 * make sure the a dnode is fully resolved instead of pointing at a symlink.
 * If the dnode passed in is not a symlink, it is returned unmodified.
 *
 * Note that absolute paths will be resolved relative to the root directory of
 * the volume. If the host is an operating system with its own VFS layer,
 * it should resolve symlinks on its own.
 *
 * If the function returns FSW_SUCCESS, *target_dno_out points at a dnode that is
 * not a symlink. The caller is responsible for calling fsw_dnode_release on it.
**/

fsw_status_t fsw_dnode_resolve (
    struct fsw_dnode  *dno,
    struct fsw_dnode **target_dno_out
) {
    fsw_status_t    status = FSW_NOT_FOUND;
    struct fsw_string target_name;
    struct fsw_dnode *target_dno;

    // Max Link Count for Linux Kernel is 40
    int link_count = 40;

    fsw_dnode_retain (dno);

    while (--link_count > 0) {
        // Get full information
        status = fsw_dnode_fill(dno);
        if (status) {
            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: 'FSW_SUCCESS'\n"
                )
            ));

            goto errorexit;
        }

        if (dno->type != FSW_DNODE_TYPE_SYMLINK) {
            // Return Non-symlink Target Found
            *target_dno_out = dno;
            return FSW_SUCCESS;
        }

        if (dno->parent == NULL) {
            // Safety measure  ... Cannot happen in theory
            FSW_MSG_LEVEL_1((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: 'FSW_NOT_FOUND' (dno->parent==NULL)\n"
                )
            ));

            status = FSW_NOT_FOUND;
            goto errorexit;
        }

        FSW_MSG_LEVEL_2((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Resolve Symlink:- 'Begin'\n"
            )
        ));

        // Read Link Target
        status = fsw_dnode_readlink (
            dno, &target_name
        );
        if (!status) {
            // Resolve Link Target
            status = fsw_dnode_lookup_path (
                dno->parent,
                &target_name, '/',
                &target_dno
            );
        }

        fsw_strfree (&target_name);

        if (status) {
            FSW_MSG_LEVEL_1((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_resolve ... Leaving with Status '%d' Error\n"
                ), status
            ));

            goto errorexit;
        }

        FSW_MSG_LEVEL_2((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Resolve Symlink:- 'Ended'\n"
            )
        ));

        // Make 'target_dno' the new dno
        fsw_dnode_release (dno);
        dno = target_dno;   // Already retained
    }

    if (link_count == 0) {
        FSW_MSG_LEVEL_1((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: 'FSW_NOT_FOUND' (link_count==0)\n"
            )
        ));

        status = FSW_NOT_FOUND;
    }

errorexit:
    fsw_dnode_release (dno);

    #if FSW_DEBUG_LEVEL >= 1
    if (status) {
        FSW_MSG_LEVEL_1((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Leaving with Status '%d' Error\n"
            ), status
        ));
    }
    else {
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: FSW_SUCCESS\n"
            )
        ));
    }
    #endif

    return status;
}

/**
 * Set up a shandle (storage handle) to access a file's data. This function
 * is called by the host driver and by the core when they need to access a
 * file's data. It is also used in accessing the raw data of directories
 * and symlinks if the filesystem uses the same mechanisms for storing
 * the data of those items.
 *
 * The storage for the fsw_shandle structure is provided by the caller.
 * The dnode and pos fields may be accessed, pos may also be written to set
 * the file pointer. The file's data size is available as shand->dnode->size.
 *
 * If this function returns FSW_SUCCESS, the caller must call fsw_shandle_close
 * to release the dnode reference held by the shandle.
**/

fsw_status_t fsw_shandle_open (
    struct fsw_dnode   *dno,
    struct fsw_shandle *shand
) {
    fsw_status_t    status;
    struct fsw_volume *vol = dno->vol;

    // Read full dnode information into memory
    status = vol->fstype_table->dnode_fill(vol, dno);
    if (status) {
        FSW_MSG_LEVEL_1((
            FSW_MSG_STR(
                "FSW_CORE: fsw_shandle_open ... Leaving with Status '%d' Error\n"
            ), status
        ));

        return status;
    }

    // Setup shandle
    fsw_dnode_retain (dno);

    shand->dnode = dno;
    shand->pos = 0;
    shand->extent.type = FSW_EXTENT_TYPE_INVALID;

    FSW_MSG_LEVEL_3((
        FSW_MSG_STR(
            "FSW_CORE: fsw_shandle_open ... Leaving with Status: 'FSW_SUCCESS'\n"
        )
    ));

    return FSW_SUCCESS;
}

/**
 * Close a shandle after accessing the dnode's data. This function is called by
 * the host driver or core functions when they are finished with accessing
 * a file's data. It releases the dnode reference and frees any buffers
 * associated with the shandle itself. The dnode is only released
 * if this was the last reference using it.
**/

void fsw_shandle_close(
    struct fsw_shandle *shand
) {
    if (shand->extent.buffer &&
        shand->extent.type   == FSW_EXTENT_TYPE_BUFFER
    ) {
        FSW_DO_FREE(shand->extent.buffer);
    }
    fsw_dnode_release (shand->dnode);
}

/**
 * Read data from a shandle (storage handle for a dnode). This function is
 * called by the host driver or internally when data is read from a file.
**/

fsw_status_t fsw_shandle_read (
    struct fsw_shandle *shand,
    fsw_u32 *buffer_size_inout,
    void *buffer_in
) {
    fsw_status_t       status;
    struct fsw_dnode  *dno = shand->dnode;
    struct fsw_volume *vol = dno->vol;
    fsw_u8            *block_buffer;
    fsw_u8            *buffer;
    fsw_u64            pos_in_extent;
    fsw_u64            pos_in_physblock;
    fsw_u64            buflen, copylen, pos;
    fsw_u64            remaining_file_data;
    fsw_u64            log_bno, phys_bno;
    fsw_u32            cache_level;
    BOOLEAN            void_hole;


    if (shand->pos >= dno->size) {
        // Already at EOF
        *buffer_size_inout = 0;

        return FSW_SUCCESS;
    }

    // Initialize vars
    pos = shand->pos;
    buffer = buffer_in;
    remaining_file_data = dno->size - pos;

    cache_level = (
        dno->type != FSW_DNODE_TYPE_FILE
    ) ? 1 : 0;

    // Amount requested by caller is 'buflen'.
    // Only want to read to end of the file.
    buflen = *buffer_size_inout;
    if (buflen > remaining_file_data) {
        buflen = remaining_file_data;
    }
    while (buflen > 0) {
        void_hole = 0;

        FSW_MSG_LEVEL_2((
            FSW_MSG_STR(
                "FSW_CORE: fsw_shandle_read ... buflen==%llu this_pos==%llu\n"
            ),
            (unsigned long long) buflen,
            (unsigned long long) pos
        ));

        // Get extent for current logical block
        log_bno = FSW_U64_DIV(pos, vol->log_blocksize);
        FSW_MSG_LEVEL_3((
            FSW_MSG_STR(
                "FSW_CORE: fsw_shandle_read ... log_bno==%llu log_start==%llu\n"
            ),
            (unsigned long long) log_bno,
            (unsigned long long) shand->extent.log_start
        ));

        if (shand->extent.type == FSW_EXTENT_TYPE_INVALID ||
            log_bno <  shand->extent.log_start            ||
            log_bno >= shand->extent.log_start + shand->extent.log_count
        ) {

            if (shand->extent.buffer &&
                shand->extent.type   == FSW_EXTENT_TYPE_BUFFER
            ) {
                FSW_DO_FREE(shand->extent.buffer);
            }

            // Get extents from filesystem
            shand->extent.log_start = log_bno;
            status = vol->fstype_table->get_extent (
                vol, dno, &shand->extent
            );
            if (status) {
                void_hole = (
                    status == FSW_IO_ERROR &&
                    shand->extent.type == FSW_EXTENT_TYPE_INVALID
                );
                if (!void_hole) {
                    shand->extent.type = FSW_EXTENT_TYPE_INVALID;

                    FSW_MSG_LEVEL_1((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_shandle_read ... Leaving with Status '%d' Error (Invalid Storage Handle Extents)\n"
                        ), status
                    ));

                    return status;
                }

                // Got Sparse Hole
                FSW_MSG_LEVEL_1((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_shandle_read ... Located Sparse Hole at log_start==%llu\n"
                    ), (unsigned long long) shand->extent.log_start
                ));
            }
        }

        pos_in_extent = pos - shand->extent.log_start * vol->log_blocksize;

        // Dispatch by extent type
        if (shand->extent.type == FSW_EXTENT_TYPE_PHYSBLOCK) {
            // Convert to physical block number and offset
            phys_bno = FSW_U64_DIV(
                pos_in_extent,
                vol->phys_blocksize
            ) + shand->extent.phys_start;
            pos_in_physblock = pos_in_extent & (vol->phys_blocksize - 1);

            copylen = vol->phys_blocksize - pos_in_physblock;
            if (copylen > buflen) copylen = buflen;

            FSW_MSG_LEVEL_3((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_shandle_read ... phys_blocksize==%llu this_copylen==%llu pos_in_physblock==%llu\n"
                ),
                (unsigned long long) vol->phys_blocksize,
                (unsigned long long) copylen,
                (unsigned long long) pos_in_physblock
            ));

            // Get physical block
            status = fsw_block_get (
                vol, phys_bno,
                cache_level,
                (void **) &block_buffer
            );
            if (status) {
                FSW_MSG_LEVEL_1((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_shandle_read ... Leaving with Status '%d' Error ('fsw_block_get' failure)\n"
                    ), status
                ));

                return status;
            }

            // Copy data from physical block
            FSW_DO_MEMCPY(
                buffer,
                block_buffer + pos_in_physblock,
                copylen
            );
            fsw_block_release (
                vol, phys_bno,
                block_buffer
            );
        }
        else {
            // FSW_EXTENT_TYPE_BUFFER/SPARSE/INVALID
            copylen = shand->extent.log_count * vol->log_blocksize - pos_in_extent;
            if (copylen > buflen) copylen = buflen;

            if (shand->extent.type == FSW_EXTENT_TYPE_BUFFER) {
                FSW_DO_MEMCPY(
                    buffer,
                    ((fsw_u8 *)shand->extent.buffer) + pos_in_extent,
                    copylen
                );
            }
            else {
                // FSW_EXTENT_TYPE_SPARSE/INVALID
                FSW_DO_MEMZERO(buffer, copylen);

                #if FSW_DEBUG_LEVEL >= 1
                if (void_hole) {
                    FSW_MSG_LEVEL_1((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_shandle_read ... Plugged Sparse Hole at log_start==%llu (log_count==%llu)\n"
                        ),
                        (unsigned long long) shand->extent.log_start,
                        (unsigned long long) shand->extent.log_count
                    ));
                }

                FSW_MSG_LEVEL_3((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_shandle_read ... this_copylen==%llu pos_in_extent==%llu\n"
                    ),
                    (unsigned long long) copylen,
                    (unsigned long long) pos_in_extent
                ));
                #endif
            }
        }

        buffer += copylen;
        pos    += copylen;

        buflen = (
            buflen > copylen
        ) ? buflen - copylen : 0;

        #if FSW_DEBUG_LEVEL >= 2
        if (buflen > 0) {
            FSW_MSG_LEVEL_2((
                FSW_MSG_STR("****  *  *  *  ****\n\n")
            ));
        }
        else {
            FSW_MSG_LEVEL_2((
                FSW_MSG_STR("=======  *  =======\n\n")
            ));
        }
        #endif
    } // while

    *buffer_size_inout = (fsw_u32)(pos - shand->pos);
    shand->pos = pos;

    return FSW_SUCCESS;
}
