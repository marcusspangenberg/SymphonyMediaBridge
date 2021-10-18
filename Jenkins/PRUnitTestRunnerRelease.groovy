@Library('SFE-RTC-pipeline') _

cmakeBuildType = "Release"

abortPreviousRunningBuilds()

notifyPRStatus("https://github.com/SymphonyOSF/SymphonyMediaBridge", "Build and Unit Testing of ${env.BRANCH_NAME} [$cmakeBuildType]") {
    node('be-integration') {
        stage('Checkout') {
            checkout scm
        }
        prRunner = load('Jenkins/PRUnitTestRunner.groovy')
        prRunner.run(cmakeBuildType)
    }
}
