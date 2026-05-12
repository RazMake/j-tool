#ifndef JUMP_UPDATE_H
#define JUMP_UPDATE_H

/*
 * Check GitHub Releases for a newer version.
 * If found, download the release zip, extract the executables,
 * and replace the current ones in-place.
 *
 * Returns J_EXIT_OK on success (or if already up to date),
 *         J_EXIT_RUNTIME_ERROR on failure.
 */
int jump_update(void);

#endif /* JUMP_UPDATE_H */
