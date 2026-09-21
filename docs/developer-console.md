# 개발자 콘솔

물리 Grave 키(보통 ` 키이며 Shift와 함께 ~)로 열고 닫는다. Enter로 명령 실행, Backspace로 한 글자 삭제, Escape로 닫는다. 입력은 인쇄 가능한 ASCII만 받으며 최대 256자, 출력 기록은 최근 64줄이다. 긴 입력은 끝부분을 보여 준다. 키 반복·자동 완성·기록 탐색은 없다.

| 명령 | 출력 |
| --- | --- |
| `drawdebug 1` / `drawdebug 0` | `Debug HUD enabled` / `Debug HUD disabled` |
| `vsync 1` / `vsync 0` | `VSync enabled` / `VSync disabled` |
| `help` | 등록된 네 명령의 사용법 |
| `clear` | 출력 기록만 지움 |

명령 이름은 대소문자를 구분하지 않는다. 앞뒤·연속 공백은 허용하고 빈 줄은 무시한다. 실행한 줄은 `> vsync 1`처럼 기록된다. 잘못된 인자는 `Usage: vsync <0|1>`, 없는 명령은 `Unknown command: foo`처럼 표시된다.

콘솔 동안 이동·점프 입력을 차단하고 열고 닫을 때 기존 입력을 초기화한다. Alt+Enter 창 전환, F12 캡처, F3 HUD 토글은 계속 동작한다. 콘솔 패널은 HUD 위에 겹쳐 그리며 F12 화면에도 포함된다. **시뮬레이션은 멈추지 않는다.** 공중에 있으면 계속 떨어진다.

VSync는 기존 Display API를 사용한다. 켜면 수동 제한 0, 끄면 60이며 HUD도 현재 값을 반영한다. 콘솔은 지역 재로딩·파일 실행·일시정지·설정 저장을 지원하지 않는다.
