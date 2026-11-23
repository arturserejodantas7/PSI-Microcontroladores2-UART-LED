/*
 * kl25z_tpm_fixup.h - Incluido em todos os arquivos quando o PWM por TPM
 * esta habilitado no KL25Z (ver CMakeLists.txt).
 *
 * O MKL25Z4_features.h do hal_nxp nao define esta macro, que o fsl_tpm.h
 * usa em TPM_MAX_COUNTER_VALUE() e TPM_SetTimerPeriod(). Os TPM do KL25Z
 * tem contador de 16 bits, portanto o valor correto e 0.
 */
#ifndef KL25Z_TPM_FIXUP_H_
#define KL25Z_TPM_FIXUP_H_

#ifndef FSL_FEATURE_TPM_HAS_32BIT_COUNTERn
#define FSL_FEATURE_TPM_HAS_32BIT_COUNTERn(x) (0)
#endif

#endif
