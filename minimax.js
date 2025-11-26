// 1. Alpha-Beta Pruning 적용 Minimax 알고리즘
// 이 함수는 재귀 호출을 통해 수를 탐색하며, alpha(최대 하한)와 beta(최소 상한) 값을 비교하여 불필요한 가지(Branch)를 잘라내어(Pruning) 연산 속도를 최적화한다.

// gameLogic 모듈에서 필요한 함수 로드
const {
    isValidMove,
    checkWin,
    evaluateBoard,
    getPossibleMoves
} = require('./gameLogic');

// Alpha-Beta Pruning을 적용한 Minimax 알고리즘
function minimax(board, depth, alpha, beta, isMaximizing, aiColor) {
    const opponent = aiColor === 'black' ? 'white' : 'black';

    // 1. 기저 사례(Base Case): 깊이 제한 도달 시 보드 평가 점수 반환
    if (depth === 0) {
        return { score: evaluateBoard(board, aiColor) };
    }

    // 탐색 후보지 선정 (깊이에 따라 탐색 범위 조절)
    const possibleMoves = getPossibleMoves(board, depth > 2 ? 10 : 15);

    if (possibleMoves.length === 0) {
        return { score: evaluateBoard(board, aiColor) };
    }

    // 2. Maximizing Player (AI 차례): 최대 점수 탐색
    if (isMaximizing) {
        let maxScore = -Infinity;
        let bestMove = null;

        for (const move of possibleMoves) {
            const { row, col } = move;

            board[row][col] = aiColor; // 가상 착수

            // 즉시 승리하는 수라면 즉시 반환 (최적화)
            if (checkWin(board, row, col, aiColor)) {
                board[row][col] = null;
                return { score: 1000000, row, col };
            }

            // 재귀 호출 (상대방 턴으로 넘김)
            const result = minimax(board, depth - 1, alpha, beta, false, aiColor);

            board[row][col] = null; // 착수 취소 (Backtracking)

            if (result.score > maxScore) {
                maxScore = result.score;
                bestMove = { row, col };
            }
            // Alpha 값 갱신 및 Beta Cutoff
           alpha = Math.max(alpha, result.score);
            if (beta <= alpha) {
                break; // 더 이상 탐색 불필요
            }
        }
        return { score: maxScore, ...bestMove };
    }
    // 3. Minimizing Player (상대방 차례): 최소 점수 탐색 (상대에게 유리한 수)
    else {
        let minScore = Infinity;
        let bestMove = null;

        for (const move of possibleMoves) {
            const { row, col } = move;

            board[row][col] = opponent; // 상대 가상 착수

            if (checkWin(board, row, col, opponent)) {
                board[row][col] = null;
                return { score: -1000000, row, col };
            }

            // 재귀 호출 (AI 턴으로 넘김)
            const result = minimax(board, depth - 1, alpha, beta, true, aiColor);

            board[row][col] = null;

            if (result.score < minScore) {
                minScore = result.score;
                bestMove = { row, col };
            }

            // Beta 값 갱신 및 Alpha Cutoff
            beta = Math.min(beta, result.score);
            if (beta <= alpha) {
                break;
            }
        }
        return { score: minScore, ...bestMove };
    }
}
