
# La matrice des combinaisons
		Projection		Lentille	Contrôleur		Parent		Gizmo	Statut  
	1	perspective		fov			FPS				interdit	oui		✅  
	2	perspective		fov			Follow			interdit	oui		✅  
	3	perspective		filmback	aucun			permis		oui		✅  
	4	orthographic	—			aucun			permis		oui		✅  
	5	orthographic	—			FPS				interdit	oui		✅  
	6	orthographic	—			Follow			interdit	oui		✅  
	7	perspective		fov			aucun			permis		non		✅ observateur  
	8	—               —			FPS + Follow	—			—		❌ interdit  

Deux axes indépendants, à ne jamais confondre :
* Camera.active / Camera.priority — sélection de la lentille qui rend la vue.
* CameraFPS.enabled / CameraFollow.enabled — autorisation d'écrire dans le transform.

Une caméra peut être active: false avec un contrôleur enabled: true : elle bouge sans être regardée. L'inverse est également valide.

# 1 — Perspective + FPS
```
{
  "id": "FPS_Camera",
  "_note": "PAS de parent : le controleur ecrit en local, un parent ajouterait sa matrice par-dessus",
  "components": {
    "Transform": {
      "translation": [ 0.0, 5.0, 40.0 ],
      "rotation": [ 0.0, 0.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "perspective",
      "lens": "fov",
      "fov": 45.0,
      "near": 0.1,
      "infiniteFar": true,
      "active": true,
      "priority": 10,
      "gizmo": { "length": 3.0 }
    },
    "CameraFPS": {
      "enabled": true,
      "moveSpeed": 15.0,
      "mouseSensitivity": 0.15,
      "lockVertical": false,
      "pitchLimit": 89.0
    }
  }
}
```
Pas de far : infiniteFar le rend inopérant, et ton parse t'avertit si tu le laisses.

# 2 — Perspective + Follow
```
{
  "id": "Follow_Camera",
  "_note": "PAS de parent : CameraFollowSystem produit une position MONDE",
  "components": {
    "Transform": {
      "translation": [ 0.0, 5.0, 0.0 ],
      "rotation": [ 0.0, 0.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "perspective",
      "lens": "fov",
      "fov": 45.0,
      "near": 0.1,
      "far": 2000.0,
      "infiniteFar": false,
      "active": true,
      "priority": 5,
      "gizmo": { "length": 3.0 }
    },
    "CameraFollow": {
      "enabled": true,
      "target": "Cube1",
      "offset": [ 0.0, 5.0, -35.0 ],
      "smoothSpeed": 5.0,
      "lookAtHeight": 0.0
    }
  }
}
```
priority: 5 contre 10 pour la FPS : les deux sont active, la FPS gagne. Baisse-la à 1 pour basculer.

# 3 — Perspective sténopé, statique 
```
{
  "id": "Cinematic_Camera",
  "components": {
    "Transform": {
      "translation": [ -20.0, 8.0, 25.0 ],
      "rotation": [ -10.0, 35.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "perspective",
      "lens": "filmback",
      "focalLength": 50.0,
      "filmHeight": 24.0,
      "gateFit": "fill",
      "near": 0.1,
      "far": 500.0,
      "infiniteFar": false,
      "active": false,
      "priority": 0,
      "gizmo": { "length": 4.0 }
    }
  }
}
```
Ni fov ici : CameraFovY() le dérive de la focale. L'écrire déclencherait WarnUnread.

# 4 — Orthographique, vue de dessus
```
{
  "id": "Top_Camera",
  "components": {
    "Transform": {
      "translation": [ 0.0, 150.0, 0.0 ],
      "rotation": [ -90.0, 0.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "orthographic",
      "orthoHeight": 120.0,
      "near": 1.0,
      "far": 400.0,
      "active": false,
      "priority": 0,
      "gizmo": { "length": 30.0 }
    }
  }
}
```
far obligatoire et réellement consommé. length: 30 et non 3 — sinon la boîte devient une dalle illisible.

# 5 — Orthographique + FPS
```
{
  "id": "Ortho_Roam_Camera",
  "_note": "Le FPS deplace une ortho : le zoom passe par orthoHeight, PAS par fov",
  "components": {
    "Transform": {
      "translation": [ 0.0, 40.0, 60.0 ],
      "rotation": [ -30.0, 0.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "orthographic",
      "orthoHeight": 60.0,
      "near": 0.5,
      "far": 500.0,
      "active": false,
      "priority": 0,
      "gizmo": { "length": 15.0 }
    },
    "CameraFPS": {
      "enabled": true,
      "moveSpeed": 20.0,
      "mouseSensitivity": 0.15,
      "lockVertical": false,
      "pitchLimit": 89.0
    }
  }
}
```
Combinaison légitime mais contre-intuitive : avancer ne grossit rien, la section est constante. C'est orthoHeight qui joue le rôle du zoom.

# 6 — Orthographique + Follow (isométrique)
```
{
  "id": "Iso_Camera",
  "components": {
    "Transform": {
      "translation": [ 0.0, 0.0, 0.0 ],
      "rotation": [ -35.264, 45.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "orthographic",
      "orthoHeight": 30.0,
      "near": 1.0,
      "far": 300.0,
      "active": false,
      "priority": 0,
      "gizmo": { "length": 10.0 }
    },
    "CameraFollow": {
      "enabled": true,
      "target": "Cube1",
      "offset": [ 60.0, 60.0, -60.0 ],
      "smoothSpeed": 5.0,
      "lookAtHeight": 1.0
    }
  }
}
```
−35.264° est atan(1/√2) — l'angle isométrique exact. Combiné à 45° de yaw, il produit la projection des jeux de stratégie classiques.

# 7 — L'observateur, sans gizmo
```
{
  "id": "Overview_Camera",
  "_note": "Rend le viewport DROIT. Aucun bloc gizmo : elle ne verrait jamais le sien",
  "components": {
    "Transform": {
      "translation": [ 0.0, 0.0, 120.0 ],
      "rotation": [ 0.0, 0.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "perspective",
      "lens": "fov",
      "fov": 60.0,
      "near": 1.0,
      "infiniteFar": true,
      "active": false,
      "priority": 0
    }
  }
}
```
L'absence de gizmo met m_gizmoLength à 0 : aucune entité créée. Économie d'un cas dégénéré.

# Les trois interdits
## 8 — Deux contrôleurs sur la même entité. CheckControllerExclusivity doit sauter (bug 25).
```
"CameraFPS":    { "enabled": true, "moveSpeed": 15.0 },
"CameraFollow": { "enabled": true, "target": "Cube1" }
```

## 9 — Un parent sous un contrôleur. Le contrôleur écrit un transform absolu ; WorldTransformSystem y composerait la matrice du parent. Dérive silencieuse.
```
"parent": "Player",
"components": { "CameraFPS": { "enabled": true } }
```

## 10 — Clés hors branche. WarnUnread doit parler pour chacune.
```
"projection": "orthographic",
"fov": 45.0,            // sans objet en ortho
"infiniteFar": true,    // sans objet en ortho
"orthoHeight": 120.0
```
