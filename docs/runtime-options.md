# 런타임 옵션

`project_slit.exe [region] [options]`. region을 생략하면 `practice_room`이며 옵션 앞뒤 어디에나 둘 수 있다. Region 이름은 소문자·숫자·밑줄을 쓴다.

| 옵션 | 동작 |
| --- | --- |
| `--tileset <name>` | 지형 타일셋 지정. 명시적 장식 옵션이 없으면 지역 기본 장식을 끈다. |
| `--decorations <json>` | 지정 JSON으로 장식 대체. `none`은 장식 끔. 타일셋 override보다 우선한다. 경로는 작업 디렉터리부터 상위로 탐색하고 이미지 경로는 JSON 기준이다. |
| `--debug-hud` | 디버그 HUD를 켠 채 시작한다. |
| `--vsync` | 수직 동기화를 켜고 수동 프레임 제한을 해제한다. 기본은 OFF + 수동 60FPS 제한이다. |
| `--help` | 창 없이 도움말을 출력하고 종료한다. |

값 없는 플래그 중복은 무시한다. 값 옵션 중복·빈 값·누락, 알 수 없는 옵션, 두 번째 region은 오류다. 장식/타일셋 옵션이 모두 없으면 Region 기본 장식을 사용한다(기본 키가 없으면 끔).

## 조작

- A/D·좌우 방향키: 이동, 지상 같은 방향 더블탭 후 홀드: Sprint.
- Space: 점프. 공중 방향 더블탭: 대쉬. W/Up·S/Down 홀드: Slow/Fast Fall.
- F3: HUD 켜기/끄기. 표시 좌표는 충돌 박스 발 중심이다.
- F12: 현재 화면 PNG와 좌표 JSON을 저장소 `build/preview/captured_XXXX.*`에 저장한다. JSON의 playerPosition은 충돌 중심으로 HUD 발 좌표와 다르다.
- Alt+Enter: 테두리 없는 전체 화면 ↔ 창 모드. Alt+F4: 종료.

## 실행 예제

저장소 루트에서 실행한다. 아래 순서는 기본 / HUD / VSync / HUD+VSync / 시험 장식 / 시험 장식+HUD+VSync다.

```powershell
.\build\Release\project_slit.exe avatar_lake
.\build\Release\project_slit.exe avatar_lake --debug-hud
.\build\Release\project_slit.exe avatar_lake --vsync
.\build\Release\project_slit.exe avatar_lake --debug-hud --vsync
.\build\Release\project_slit.exe avatar_lake --decorations assets/decorations/avatar_lake_fossil04B_trial.json
.\build\Release\project_slit.exe avatar_lake --decorations assets/decorations/avatar_lake_fossil04B_trial.json --debug-hud --vsync
```

## 동기화

VSync OFF는 WGL swap interval 0 + 수동 60FPS 제한, ON은 interval 1 + 수동 제한 0이다. 창 재생성 뒤 선택 상태를 재적용한다. HUD의 ON/OFF는 Display의 현재 요청 상태이며 드라이버 제어판이 실제 동작을 강제로 덮을 수 있다. SFML OpenGL을 사용하므로 DXGI Present나 tearing 플래그는 사용하지 않는다. 물리의 delta time과 0.05초 클램프는 바뀌지 않는다.

### 실제 플레이 결과 (2026-09-21)

- 기본 VSync OFF 에서는 기존 수동 60FPS 제한이 그대로 유지된다.
- `--vsync` 를 쓰면 실제 모니터에서 티어링이 사라지는 것을 사용자가 확인했다.
- VSync ON 에서 입력 반응이 약간 둔해진 듯하다는 사용자 체감이 있었다. 아직 계측된 결함이 아니며, 필요해지면 따로 측정한다.
- 반응성을 우선하면 기본 OFF, 티어링 제거를 원하면 `--vsync` 를 고른다.

인게임 콘솔은 미구현이다.
