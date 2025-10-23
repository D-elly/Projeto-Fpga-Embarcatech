#include "FatFsPort.h"

extern "C" {
#include "ff.h"
#include "diskio.h"
}

namespace {
cartao_sd::DriverCartaoSd *driverRegistrado = nullptr;
constexpr BYTE UNIDADE_UNICA = 0;
}

namespace cartao_sd {

void registrarDriverFatFs(DriverCartaoSd *driver) {
    driverRegistrado = driver;
}

} // namespace cartao_sd

extern "C" {

DSTATUS disk_status(BYTE unidade) {
    if (unidade != UNIDADE_UNICA) {
        return STA_NOINIT;
    }

    if (driverRegistrado == nullptr) {
        return STA_NOINIT;
    }

    return driverRegistrado->estaInicializado() ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE unidade) {
    if (unidade != UNIDADE_UNICA) {
        return STA_NOINIT;
    }

    if (driverRegistrado == nullptr) {
        return STA_NOINIT;
    }

    bool iniciou = driverRegistrado->iniciar();
    return iniciou ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE unidade, BYTE *buffer, LBA_t setor, UINT quantidade) {
    if (unidade != UNIDADE_UNICA || driverRegistrado == nullptr) {
        return RES_PARERR;
    }

    if (buffer == nullptr || quantidade == 0) {
        return RES_PARERR;
    }

    bool leu = driverRegistrado->lerSetores(buffer, static_cast<uint32_t>(setor), quantidade);
    return leu ? RES_OK : RES_ERROR;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE unidade, const BYTE *buffer, LBA_t setor, UINT quantidade) {
    if (unidade != UNIDADE_UNICA || driverRegistrado == nullptr) {
        return RES_PARERR;
    }

    if (buffer == nullptr || quantidade == 0) {
        return RES_PARERR;
    }

    bool escreveu = driverRegistrado->escreverSetores(buffer, static_cast<uint32_t>(setor), quantidade);
    return escreveu ? RES_OK : RES_ERROR;
}
#endif

DRESULT disk_ioctl(BYTE unidade, BYTE comando, void *buffer) {
    if (unidade != UNIDADE_UNICA || driverRegistrado == nullptr) {
        return RES_PARERR;
    }

    switch (comando) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_BLOCK_SIZE: {
            if (buffer == nullptr) {
                return RES_PARERR;
            }
            DWORD *destino = reinterpret_cast<DWORD *>(buffer);
            *destino = 1u;
            return RES_OK;
        }
        case GET_SECTOR_COUNT: {
            if (buffer == nullptr) {
                return RES_PARERR;
            }
            uint64_t setores = driverRegistrado->obterQuantidadeSetores();
            if (setores == 0u) {
                return RES_ERROR;
            }
            *reinterpret_cast<LBA_t *>(buffer) = static_cast<LBA_t>(setores);
            return RES_OK;
        }
        default:
            return RES_PARERR;
    }
}

} // extern "C"
