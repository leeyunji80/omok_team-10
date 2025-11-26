// Minimax 알고리즘 기반 오목 AI

const {
    isValidMove,
    checkWin,
    evaluateBoard,
    getPossibleMoves
} = require('./gameLogic');

// Alpha-Beta Pruning을 적용한 Minimax 알고리즘
function minimax(board, depth, alpha, beta, isMaximizing, aiColor) {
    const opponent = aiColor === 'black' ? 'white' : 'black';

    // 깊이가 0이거나 게임이 끝난 경우
    if (depth === 0) {
        return { score: evaluateBoard(board, aiColor) };
    }

    const possibleMoves = getPossibleMoves(board, depth > 2 ? 10 : 15);

    if (possibleMoves.length === 0) {
        return { score: evaluateBoard(board, aiColor) };
    }

    if (isMaximizing) {
        let maxScore = -Infinity;
        let bestMove = null;

        for (const move of possibleMoves) {
            const { row, col } = move;

            // 착수
            board[row][col] = aiColor;

            // 즉시 승리하는 수인지 확인
            if (checkWin(board, row, col, aiColor)) {
                board[row][col] = null;
                return { score: 1000000, row, col };
            }

            // 재귀 호출
            const result = minimax(board, depth - 1, alpha, beta, false, aiColor);

            // 착수 취소
            board[row][col] = null;

            if (result.score > maxScore) {
                maxScore = result.score;
                bestMove = { row, col };
            }

            alpha = Math.max(alpha, result.score);
            if (beta <= alpha) {
                break; // Beta cutoff
            }
        }

        return { score: maxScore, ...bestMove };
    } else {
        let minScore = Infinity;
        let bestMove = null;

        for (const move of possibleMoves) {
            const { row, col } = move;

            // 착수
            board[row][col] = opponent;

            // 상대가 즉시 승리하는 수인지 확인
            if (checkWin(board, row, col, opponent)) {
                board[row][col] = null;
                return { score: -1000000, row, col };
            }

            // 재귀 호출
            const result = minimax(board, depth - 1, alpha, beta, true, aiColor);

            // 착수 취소
            board[row][col] = null;

            if (result.score < minScore) {
                minScore = result.score;
                bestMove = { row, col };
            }

            beta = Math.min(beta, result.score);
            if (beta <= alpha) {
                break; // Alpha cutoff
            }
        }

        return { score: minScore, ...bestMove };
    }
}

// AI의 최적 착수 찾기
function findBestMove(board, aiColor, difficulty = 'medium') {
    // 난이도별 깊이 설정
    const depthMap = {
        'easy': 2,
        'medium': 3,
        'hard': 4
    };

    const depth = depthMap[difficulty] || 3;

    // 상대 막기 우선 확인
    const opponent = aiColor === 'black' ? 'white' : 'black';
    const possibleMoves = getPossibleMoves(board, 20);

    // 상대의 4목을 막아야 하는 경우 찾기
    for (const move of possibleMoves) {
        const { row, col } = move;
        board[row][col] = opponent;
        if (checkWin(board, row, col, opponent)) {
            board[row][col] = null;
            return { row, col }; // 즉시 막기
        }
        board[row][col] = null;
    }

    // AI가 즉시 이길 수 있는 경우 찾기
    for (const move of possibleMoves) {
        const { row, col } = move;
        board[row][col] = aiColor;
        if (checkWin(board, row, col, aiColor)) {
            board[row][col] = null;
            return { row, col }; // 즉시 승리
        }
        board[row][col] = null;
    }

    // Minimax 알고리즘으로 최적 수 찾기
    const result = minimax(board, depth, -Infinity, Infinity, true, aiColor);

    // 초급 난이도의 경우 가끔 랜덤하게 실수
    if (difficulty === 'easy' && Math.random() < 0.3) {
        const randomMove = possibleMoves[Math.floor(Math.random() * Math.min(5, possibleMoves.length))];
        return randomMove;
    }

    return { row: result.row, col: result.col };
}

module.exports = {
    findBestMove,
    minimax
};