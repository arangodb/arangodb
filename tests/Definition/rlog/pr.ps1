Function global:registerTests()
{
    noteStartAndRepoState

    Write-Host "Registering tests..."

    $global:TESTSUITE_TIMEOUT = 3900

    registerTest -testname "gtest_replication2"
    registerTest -testname "replication2_server" -cluster $true
    registerTest -testname "auto" -index rlog-cluster -cluster $true -filter "tests\\js\\common\\shell\\shell-replicated-logs-cluster.js"
    registerTest -testname "auto" -index prototype-state-cluster -cluster $true -filter "tests\\js\\common\\shell\\shell-prototype-state-cluster.js"
    registerTest -testname "auto" -index shell-transaction -cluster $true -filter "tests\\js\\client\\shell\\transaction\\shell-transaction.js"
    registerTest -testname "auto" -index shell-transaction-cluster -cluster $true -filter "tests\\js\\client\\shell\\transaction\\shell-transaction-cluster.js"
    registerTest -testname "auto" -index shell-transaction-replication2-recovery -cluster $true -filter "tests\\js\\client\\shell\\transaction\\replication2_recovery\\shell-transaction-replication2-recovery.js"
    comm
}
