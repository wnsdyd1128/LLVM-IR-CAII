/*
 * RTEMS 6.0 / GR740 예제: 정적 분석 테스트용
 *
 * 의도적으로 세 가지 결함을 포함한다:
 *   1. malloc 반환값 null 체크 누락
 *   2. 스택 버퍼 과다 사용
 *   3. rtems_semaphore_obtain 반환값 무시
 */

#include <rtems.h>
#include <stdlib.h>
#include <string.h>

/* ── 결함 1: null 체크 없이 malloc 결과 사용 ───────────────────────── */
void task_init_data(void)
{
  char * buf = (char *)malloc(256);
  memset(buf, 0, 256); /* null 체크 없이 바로 사용 → NullDerefChecker */
  free(buf);
}

/* ── 결함 2: 대형 로컬 배열 → 스택 초과 위험 ──────────────────────── */
void process_telemetry(void)
{
  char local_buf[8192]; /* StackUsageAnalyzer 가 오류 감지 */
  memset(local_buf, 0, sizeof(local_buf));
}

/* ── 결함 3: RTEMS API 반환값 무시 ─────────────────────────────────── */
rtems_id g_sem;

void init_semaphore(void)
{
  rtems_semaphore_create(
    rtems_build_name('T', 'E', 'S', 'T'), 1, RTEMS_DEFAULT_ATTRIBUTES, 0,
    &g_sem); /* 반환값(rtems_status_code) 무시 → RTEMSAPIChecker */
}

void acquire_semaphore(void)
{
  rtems_semaphore_obtain(g_sem, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
  /* 반환값 무시 */
}

/* ── RTEMS Init Task ─────────────────────────────────────────────── */
rtems_task Init(rtems_task_argument ignored)
{
  init_semaphore();
  acquire_semaphore();
  task_init_data();
  process_telemetry();
  rtems_task_delete(RTEMS_SELF);
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_MAXIMUM_TASKS 1
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT
#include <rtems/confdefs.h>