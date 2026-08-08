;=========================================================
; clearscreen.asm
;
; Assemblage : ml64 /c /Fo clearscreen.obj clearscreen.asm
;
; CONVENTION COMMUNE A TOUTES LES VERSIONS :
;   rcx = adresse du buffer
;   rdx = TAILLE EN OCTETS (et non un compteur d'iterations)
;
; Contraintes d'alignement :
;   V1  : aucune (vmovdqu)
;   V2  : buffer aligne sur 32 octets (vmovdqa)
;   V3  : buffer aligne sur 32 octets (vmovntdq ymm)
;   V4  : buffer aligne sur 64 octets (vmovntdq zmm)
;
; rdx doit etre un multiple de 32 (V1..V3) ou 64 (V4).
; Un framebuffer W*H*4 l'est toujours si W est multiple de 8.
;=========================================================

.code

;---------------------------------------------------------
; V1 - version d'origine : 1 store non aligne de 32 o / tour
;---------------------------------------------------------
	align 16
CleanScreenV1 proc

	test	rdx,rdx
	jz	v1_done

	vpxor	ymm0,ymm0,ymm0
	xor	rax,rax
v1_loop:
	vmovdqu	ymmword ptr [rcx+rax],ymm0
	add	rax,32
	cmp	rax,rdx
	jb	v1_loop

	vzeroupper	; Quand vous utilisez YMM (256-bit AVX) ou ZMM (512-bit AVX-512), les bits supérieurs des registres ZMM restent "actifs". 
				; Si après votre fonction, le code appelle des instructions SSE (XMM) ou des opérations FPU standard sans nettoyer d'abord, 
				; le CPU doit resynchroniser l'état — pénalité de 15-30 cycles (voir plus selon le CPU).
				; vzeroupper zéroïse les bits 256-511 des registres ZMM, ce qui signale "j'utilise plus les registres larges, optimise pour SSE". C'est gratuit (~1 cycle) mais essentiel.
v1_done:
	ret

CleanScreenV1 endp


;---------------------------------------------------------
; V2 - AVX2 deroule x4, stores alignes normaux (passent par le cache)
;---------------------------------------------------------
	align 16
CleanScreenV2 proc

	vpxor	ymm0,ymm0,ymm0
	xor	rax,rax

	mov	r8,rdx
	and	r8,-128			; portion traitee par la boucle principale
	jz	v2_tail

v2_loop:
	vmovdqa	ymmword ptr [rcx+rax],ymm0
	vmovdqa	ymmword ptr [rcx+rax+32],ymm0
	vmovdqa	ymmword ptr [rcx+rax+64],ymm0
	vmovdqa	ymmword ptr [rcx+rax+96],ymm0
	add	rax,128
	cmp	rax,r8
	jb	v2_loop

v2_tail:
	cmp	rax,rdx
	jae	v2_done
v2_tloop:
	vmovdqa	ymmword ptr [rcx+rax],ymm0
	add	rax,32
	cmp	rax,rdx
	jb	v2_tloop

v2_done:
	vzeroupper
	ret

CleanScreenV2 endp


;---------------------------------------------------------
; V3 - AVX2 deroule x4, stores NON TEMPORELS (contournent le cache)
;---------------------------------------------------------
	align 16
CleanScreenV3 proc

	vpxor	ymm0,ymm0,ymm0
	xor	rax,rax

	mov	r8,rdx
	and	r8,-128
	jz	v3_tail

v3_loop:
	vmovntdq ymmword ptr [rcx+rax],ymm0
	vmovntdq ymmword ptr [rcx+rax+32],ymm0
	vmovntdq ymmword ptr [rcx+rax+64],ymm0
	vmovntdq ymmword ptr [rcx+rax+96],ymm0
	add	rax,128
	cmp	rax,r8
	jb	v3_loop

v3_tail:
	cmp	rax,rdx
	jae	v3_done
v3_tloop:
	vmovntdq ymmword ptr [rcx+rax],ymm0
	add	rax,32
	cmp	rax,rdx
	jb	v3_tloop

v3_done:
	sfence				; obligatoire apres des stores non temporels
	vzeroupper
	ret

CleanScreenV3 endp


;---------------------------------------------------------
; V4 - AVX-512 deroule x4, stores NON TEMPORELS (256 o / tour)
;---------------------------------------------------------
	align 16
CleanScreenV4 proc

	vpxorq	zmm0,zmm0,zmm0
	xor	rax,rax

	mov	r8,rdx
	and	r8,-256
	jz	v4_tail

v4_loop:
	vmovntdq zmmword ptr [rcx+rax],zmm0
	vmovntdq zmmword ptr [rcx+rax+64],zmm0
	vmovntdq zmmword ptr [rcx+rax+128],zmm0
	vmovntdq zmmword ptr [rcx+rax+192],zmm0
	add	rax,256
	cmp	rax,r8
	jb	v4_loop

v4_tail:
	cmp	rax,rdx
	jae	v4_done
v4_tloop:
	vmovntdq zmmword ptr [rcx+rax],zmm0
	add	rax,64
	cmp	rax,rdx
	jb	v4_tloop

v4_done:
	sfence
	vzeroupper
	ret

CleanScreenV4 endp

	end