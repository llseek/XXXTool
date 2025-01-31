#include <errno.h>
#include <string.h>
#include <dirent.h>
#include "gpudridebug.h"
#include "utils.h"

#define DRI_DEBUG_PATH  "/sys/kernel/debug/dri"

static int parseRingInfo(struct AmdGpuRing *gpuRing, const char *name)
{
    int ret = 0;
    char *p = NULL;
    int32_t a = 0, b = 0, c = 0;

    if (!gpuRing)
        return -EINVAL;

    gpuRing->values[0] = -1;
    gpuRing->values[1] = -1;
    gpuRing->values[2] = -1;

    strncpy(gpuRing->name, name, RING_NAME_SIZE);
    strncpy(gpuRing->shortname, name + 12, RING_NAME_SIZE);
    p = gpuRing->shortname;

    if (!strncmp(p, "gfx", 3)) {
        gpuRing->type = RingType_GFX;
        if (sscanf(p, "gfx_%d", &a) == 1)
            gpuRing->values[0] = a;
    }
    else if (!strncmp(p, "sdma", 4)) {
        gpuRing->type = RingType_SDMA;
        if (sscanf(p, "sdma%d", &a) == 1)
            gpuRing->values[0] = a;
    }
    else if (!strncmp(p, "comp", 4)) {
        gpuRing->type = RingType_COMP;
        if (sscanf(p, "comp_%d.%d.%d", &a, &b, &c) == 3) {
            gpuRing->values[0] = a;
            gpuRing->values[1] = b;
            gpuRing->values[2] = c;
        }
    }
    else if (!strncmp(p, "uvd", 3)) {
        gpuRing->type = RingType_UVD;
        if (sscanf(p, "uvd_%d", &a) == 1)
            gpuRing->values[0] = a;
        else if (sscanf(p, "uvd_enc_%d.%d", &a, &b) == 2) {
            gpuRing->values[0] = a;
            gpuRing->values[1] = b;
        }
    }
    else if (!strncmp(p, "kiq", 3)) {
        gpuRing->type = RingType_KIQ;
        if (sscanf(p, "kiq_%d.%d.%d", &a, &b, &c) == 3) {
            gpuRing->values[0] = a;
            gpuRing->values[1] = b;
            gpuRing->values[2] = c;
        }
    }
    else if (!strncmp(p, "vce", 3)) {
        gpuRing->type = RingType_VCE;
        if (sscanf(p, "vce%d", &a) == 1)
            gpuRing->values[0] = a;
    }
    else {
        gpuRing->type = RingType_UNKNOW;
    }

    return ret;
}

int amdGpuRingInfo(struct GpuDevice *device, struct AmdGpuRing *gpuRing, uint32_t *count)
{
    int ret = 0;
    uint32_t ring_num = 0;
    size_t size = 0;
    DIR *dir = NULL;
    struct dirent *dirent = NULL;
    char fname[100] = {0};

    size = snprintf(fname, 100, "%s/%d", DRI_DEBUG_PATH, device->minor);
    fname[size] = '\0';

    dir = opendir(fname);
    while (dirent = readdir(dir)) {
        if (dirent->d_name[0] == '.')
            continue;
        if (!strncmp(dirent->d_name, "amdgpu_ring_", 12)) {
            if (gpuRing && &gpuRing[ring_num]) {
                ret = parseRingInfo(&gpuRing[ring_num], dirent->d_name);
                if (ret)
                    return ret;
                gpuRing[ring_num].device = device;
            }
            ring_num++;
        }
    }

    if (count)
        *count = ring_num;
    closedir(dir);
    return 0;
}

int amdGpuQueryFenceInfo(struct AmdGpuRing *ring, struct AmdGpuFenceInfo *fenceInfo)
{
    int ret = 0;
    size_t size = 0;
    int count = 0;
    int ringid = 0;
    char fname[100] = {0};
    char ringname[RING_NAME_SIZE] = {0};
    char buff[1024] = {0};
    uint32_t fence_count = 0;
    FILE *fp = NULL;

    MemClear(fenceInfo, sizeof(*fenceInfo));
    size = snprintf(fname, 100, "%s/%d/%s", DRI_DEBUG_PATH, ring->device->minor, AMDGPU_FENCE_INFO_NAME);
    fname[size] = '\0';
    fp = fopen(fname, "r");
    while (!feof(fp) && fgets(buff, 1024, fp)) {
        count = sscanf(buff, "--- ring %d (%s) ---", &ringid, ringname);
        if (count == 2) {
            fenceInfo->ringid = ringid;
            if (!strncmp(ringname, ring->shortname, strlen(ring->shortname))) {
                for (int i = 0; i < (ring->type == RingType_GFX ? 5 : 2); i++) {
                    fgets(buff, 1024, fp);
                    if (strstr(buff, "emitted")) {
                        if (sscanf(buff, "%*s %*s 0x%08x", &fence_count) == 1)
                            fenceInfo->emitted = fence_count;
                    } else if (strstr(buff, "signaled")) {
                        if (sscanf(buff, "%*s %*s %*s 0x%08x", &fence_count) == 1)
                            fenceInfo->signaled = fence_count;
                    } else if (strstr(buff, "preempted")) {
                        if (sscanf(buff, "%*s %*s 0x%08x", &fence_count) == 1)
                            fenceInfo->preempted = fence_count;
                    } else if (strstr(buff, "reset")) {
                        if (sscanf(buff, "%*s %*s 0x%08x", &fence_count) == 1)
                            fenceInfo->reset = fence_count;
                    } else if (strstr(buff, "both")) {
                        if (sscanf(buff, "%*s %*s 0x%08x", &fence_count) == 1)
                            fenceInfo->both = fence_count;
                    }
                }
            }
        }
    }
    fclose(fp);

    return ret;
}