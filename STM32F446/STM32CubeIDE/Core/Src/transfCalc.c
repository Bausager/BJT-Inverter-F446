/*
 * transfCalc.c
 *
 *  Created on: Feb 17, 2024
 *      Author: Bausa
 */
#include "transfCalc.h"

/*
 * Helper variables for calculations
 */
static float cosTemp, sinTemp, temp1, temp2, temp3;


/*
 * Function:  transf_abc_to_alphabetagamma
 * ----------------------------------------
 *	Transformation for abc to alpha-beta-gamma (Clarke Transformation)
 *
 *  const float Ua: Line-neutral voltage for Phase A
 *  const float Ub: Line-neutral voltage for Phase B
 *  const float Uc: Line-neutral voltage for Phase C
 *
 *  float* Ualpha: Pointer to alpha variable
 *  float* Ubeta: Pointer to beta variable
 *  float* Ugamma: Pointer to gamma variable
 *
 *  returns: HAL status
 */
uint8_t transf_abc_to_alphabetagamma(const float Ua, const float Ub, const float Uc, float* Ualpha, float* Ubeta, float* Ugamma){
	*Ualpha = (0.66666f*Ua) + (-0.33333f*Ub) + (-0.33333f*Uc);
	*Ubeta = (0.57735f*Ub) + (-0.57735f*Uc);
	*Ugamma = (0.33333f*Ua) + (0.33333f*Ub) + (0.33333f*Uc);

	return HAL_OK;
}

/*
 * Function:  transf_alphabetagamma_to_abc
 * ----------------------------------------
 *	Transformation for alpha-beta-gamma to abc (Inverse Clarke Transformation)
 *
 *  const float Ualpha: Alpha value
 *  const float Ubeta: Beta value
 *  const float Ugamma: Gamma value
 *
 *  float* Ua: Pointer to line-neutral voltage for Phase A
 *  float* Ub: Pointer to line-neutral voltage for Phase B
 *  float* Uc: Pointer to line-neutral voltage for Phase C
 *
 *  returns: HAL status
 */
uint8_t transf_alphabetagamma_to_abc(const float Ualpha, const float Ubeta, const float Ugamma,float* Ua, float* Ub, float* Uc){
	*Ua = Ualpha + Ugamma;
	*Ub = (-0.5f*Ualpha) + (0.86602f*Ubeta) + (Ugamma);
	*Uc = (-0.5f*Ualpha) + (-0.86602f*Ubeta) + (Ugamma);

	return HAL_OK;
}

/*
 * Function:  transf_abc_to_alphabeta
 * -----------------------------------
 *	Transformation for abc to alpha-beta (Simplified Clarke Transformation)
 *
 *  const float Ua: Line-neutral voltage for Phase A
 *  const float Ub: Line-neutral voltage for Phase B
 *  const float Uc: Line-neutral voltage for Phase C
 *
 *  float* Ualpha: Pointer to alpha variable
 *  float* Ubeta: Pointer to beta variable
 *
 *  returns: HAL status
 */
uint8_t transf_abc_to_alphabeta(const float Ua, const float Ub, const float Uc, float* Ualpha, float* Ubeta){
	*Ualpha = (0.66666f*Ua) + (-0.33333f*Ub) + (-0.33333f*Uc);
	*Ubeta = (0.57735f*Ub) + (-0.57735f*Uc);

	return HAL_OK;
}

/*
 * Function:  transf_alphabeta_to_abc
 * ----------------------------------------
 *	Transformation for alpha-beta to abc (Simplified Inverse Clarke Transformation)
 *
 *  const float Ualpha: Alpha value
 *  const float Ubeta: Beta value
 *
 *  float* Ua: Pointer to line-neutral voltage for Phase A
 *  float* Ub: Pointer to line-neutral voltage for Phase B
 *  float* Uc: Pointer to line-neutral voltage for Phase C
 *
 *  returns: HAL status
 */
uint8_t transf_alphabeta_to_abc(const float Ualpha, const float Ubeta, float* Ua, float* Ub, float* Uc){
	*Ua = Ualpha;
	*Ub = (-0.5f*Ualpha) + (0.86602f*Ubeta);
	*Uc = (-0.5f*Ualpha) + (-0.86602f*Ubeta);

	return HAL_OK;
}


/*
 * Function:  transf_alphabetagamma_to_dqz
 * ----------------------------------------
 *	Transformation for alpha-beta-gamma to dqz (Park Transformation)
 *
 *  const float Ualpha: Alpha value
 *  const float Ubeta: Beta value
 *  const float Ugamma: Gamma value
 *  const float Theta: Grid angle
 *
 *  float* Ud: Pointer to Ud variable
 *  float* Uq: Pointer to Uq variable
 *  float* Uz: Pointer to Uz variable
 *
 *  returns: HAL status
 */
uint8_t transf_alphabetagamma_to_dqz(const float Ualpha, const float Ubeta, const float Ugamma, const float Theta, float* Ud, float* Uq, float* Uz){

	cosTemp = cosf(Theta);
	sinTemp = sinf(Theta);

	*Ud = (cosTemp*Ualpha) + (sinTemp*Ubeta);
	*Uq = (-sinTemp*Ualpha) + (cosTemp*Ubeta);
	*Uz = Ugamma;

	return HAL_OK;
}

/*
 * Function:  transf_dqz_to_alphabetagamma
 * ----------------------------------------
 *	Transformation for dqz to alpha-beta-gamma (Inverse Park Transformation)
 *
 *  const float Ud: Ud value
 *  const float Uq: Uq value
 *  const float Uz: Uz value
 *  const float Theta: Grid angle
 *
 *  float* Ualpha: Pointer to alpha variable
 *  float* Ubeta: Pointer to beta variable
 *  float* Ugamma: Pointer to gamma variable
 *
 *  returns: HAL status
 */
uint8_t transf_dqz_to_alphabetagamma(const float Ud, const float Uq, const float Uz, const float Theta, float* Ualpha, float* Ubeta, float* Ugamma){

	cosTemp = cosf(Theta);
	sinTemp = sinf(Theta);

	*Ualpha = (cosTemp*Ud) + (-sinTemp*Uq);
	*Ubeta = (sinTemp*Ud) + (cosTemp*Uq);
	*Ugamma = Uz;

	return HAL_OK;
}

/*
 * Function:  transf_alphabeta_to_dq
 * ----------------------------------
 *	Transformation for alpha-beta to dq (Simplified Park Transformation)
 *
 *  const float Ualpha: Alpha value
 *  const float Ubeta: Beta value
 *  const float Theta: Grid angle
 *
 *  float* Ud: Pointer to Ud variable
 *  float* Uq: Pointer to Uq variable
 *
 *  returns: HAL status
 */
uint8_t transf_alphabeta_to_dq(const float Ualpha, const float Ubeta, const float Theta, float* Ud, float* Uq){

	cosTemp = cosf(Theta);
	sinTemp = sinf(Theta);

	*Ud = (cosTemp*Ualpha) + (sinTemp*Ubeta);
	*Uq = (-sinTemp*Ualpha) + (cosTemp*Ubeta);

	return HAL_OK;
}

/*
 * Function:  transf_dq_to_alphabeta
 * ----------------------------------------
 *	Transformation for dq to alpha-beta (Simplified Inverse Park Transformation)
 *
 *  const float Ud: Pointer to Ud value
 *  const float Uq: Pointer to Uq value
 *  const float Theta: Grid angle
 *
 *  float* Ualpha: Pointer to alpha variable
 *  float* Ubeta: Pointer to beta variable
 *
 *  returns: HAL status
 */
uint8_t transf_dq_to_alphabeta(const float Ud, const float Uq, const float Theta, float* Ualpha, float* Ubeta){

	cosTemp = cosf(Theta);
	sinTemp = sinf(Theta);

	*Ualpha = (cosTemp*Ud) + (-sinTemp*Uq);
	*Ubeta = (sinTemp*Ud) + (cosTemp*Uq);

	return HAL_OK;
}


/*
 * Function:  transf_abc_to_dqz
 * ----------------------------
 *	Transformation for abc to dqz (Clarke-Park Transformation)
 *
 *  const float Ua: Line-neutral voltage for Phase A
 *  const float Ub: Line-neutral voltage for Phase B
 *  const float Uc: Line-neutral voltage for Phase C
 *  const float Theta: Grid angle
 *
 *  float* Ud: Pointer to Ud variable
 *  float* Uq: Pointer to Uq variable
 *  float* Uz: Pointer to Uz variable
 *
 *  returns: HAL status
 */
uint8_t transf_abc_to_dqz(const float Ua, const float Ub, const float Uc, const float Theta, float* Ud, float* Uq, float* Uz){
	transf_abc_to_alphabetagamma(Ua, Ub, Uc, &temp1, &temp2, &temp3);
	transf_alphabetagamma_to_dqz(temp1, temp2, temp3, Theta, Ud, Uq, Uz);

	return HAL_OK;
}

/*
 * Function:  transf_dqz_to_abc
 * -----------------------------
 *	Transformation for dqz to abc (Inverse Clarke-Park Transformation)
 *
 *  const float Ud: Ud value
 *  const float Uq: Uq value
 *  const float Uz: Uz value
 *  const float Theta: Grid angle
 *
 *  float* Ua: Pointer to line-neutral voltage for Phase A
 *  float* Ub: Pointer to line-neutral voltage for Phase B
 *  float* Uc: Pointer to line-neutral voltage for Phase C
 *
 *  returns: HAL status
 */
uint8_t transf_dqz_to_abc(const float Ud, const float Uq, const float Uz, const float Theta, float* Ua, float* Ub, float* Uc){
	transf_dqz_to_alphabetagamma(Ud, Uq, Uz, Theta, &temp1, &temp2, &temp3);
	transf_alphabetagamma_to_abc(temp1, temp2, temp3, Ua, Ub, Uc);

	return HAL_OK;
}

/*
 * Function:  transf_abc_to_dq
 * ----------------------------
 *	Transformation for abc to dq (Simplified Clarke-Park Transformation)
 *
 *  const float Ua: Line-neutral voltage for Phase A
 *  const float Ub: Line-neutral voltage for Phase B
 *  const float Uc: Line-neutral voltage for Phase C
 *  const float Theta: Grid angle
 *
 *  float* Ud: Pointer to Ud variable
 *  float* Uq: Pointer to Uq variable
 *
 *  returns: HAL status
 */
uint8_t transf_abc_to_dq(const float Ua, const float Ub, const float Uc, const float Theta, float* Ud, float* Uq){
	transf_abc_to_alphabeta(Ua, Ub, Uc, &temp1, &temp2);
	transf_alphabeta_to_dq(temp1, temp2, Theta, Ud, Uq);

	return HAL_OK;
}

/*
 * Function:  transf_dq_to_abc
 * -----------------------------
 *	Transformation for dq to abc (Simplified Inverse Clarke-Park Transformation)
 *
 *  const float Ud: Ud value
 *  const float Uq: Uq value
 *  const float Theta: Grid angle
 *
 *  float* Ua: Pointer to line-neutral voltage for Phase A
 *  float* Ub: Pointer to line-neutral voltage for Phase B
 *  float* Uc: Pointer to line-neutral voltage for Phase C
 *
 *  returns: HAL status
 */
uint8_t transf_dq_to_abc(const float Ud, const float Uq, const float Theta, float* Ua, float* Ub, float* Uc){
	transf_dq_to_alphabeta(Ud, Uq, Theta, &temp1, &temp2);
	transf_alphabeta_to_abc(temp1, temp2, Ua, Ub, Uc);

	return HAL_OK;
}


